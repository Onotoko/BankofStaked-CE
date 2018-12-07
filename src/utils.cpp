using namespace eosio;
using namespace eosiosystem;
using namespace bank;

namespace utils
{
  //try to get beneficiary from memo, otherwise, use sender.
  name get_beneficiary(const std::string &memo, name sender)
  {
    name to = sender;
    if (memo.length() > 0)
    {
      to = name(memo.c_str());
      eosio_assert( is_account( to ), "to account does not exist");
    }
    return to;
  }

  //get active creditor from creditor table
  name get_active_creditor(uint64_t for_free)
  {
    uint64_t active = TRUE;
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto idx = c.get_index<"is.active"_n>();
    auto itr = idx.begin();
    name creditor;
    while (itr != idx.end())
    {
      if(itr->is_active != TRUE)
      {
         itr++;
         continue;
      }

      if(itr->for_free == for_free) {
        creditor = name(itr->account);
        break;
      }
      itr++;
    }
    return creditor;
  }

  //get account EOS balance
  asset get_balance(name owner)
  {
    auto sc = eosio::symbol_code("EOS");
    auto balance = eosio::token::get_balance("eosio.token"_n, owner, sc);
    // update creditor if balance is outdated
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto creditor_itr = c.find(owner.value);
    if(creditor_itr != c.end() && creditor_itr->balance != balance) {
      c.modify(creditor_itr, ram_payer, [&](auto &i) {
        i.balance = balance;
        i.updated_at = now();
      });
    }
    return balance;
  }

  //get creditor with balance >= to_delegate
  name get_qualified_paid_creditor(asset to_delegate)
  {
    uint64_t active = TRUE;
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto idx = c.get_index<"is.active"_n>();
    auto itr = idx.begin();
    name creditor;
    while (itr != idx.end())
    {
      asset balance = get_balance(name(itr->account));
      if(itr->for_free == FALSE && balance >= to_delegate) {
        creditor = name(itr->account);
        break;
      }
      itr++;
    }
    return creditor;
  }

  //get creditor income
  asset get_income(name creditor, asset price)
  {
    dividend_table c(code_account, code_account.value);
    uint64_t amount = price.amount;
    auto itr = c.find(creditor.value);
    if(itr != c.end()) {
        price.amount = amount * itr->percentage / 100;
    } else {
        price.amount = amount * DEFAULT_DIVIDENT_PERCENTAGE / 100;
    }
    return price;
  }

  void activate_creditor(name account)
  {
    creditor_table c(code_account, SCOPE_CREDITOR>>1);

    auto creditor = c.find(account.value);
    //make sure specified creditor exists
    eosio_assert(creditor != c.end(), "account not found in creditor table");

    //activate creditor, deactivate others
    auto itr = c.end();
    while (itr != c.begin())
    {
      itr--;
      if (itr->for_free != creditor->for_free)
      {
        continue;
      }

      if(itr->account==creditor->account) {
        c.modify(itr, ram_payer, [&](auto &i) {
          i.is_active = TRUE;
          i.balance = get_balance(name(itr->account));
          i.updated_at = now();
        });
      }
      else
      {
        if(itr->is_active == FALSE)
        {
           continue;
        }
        c.modify(itr, ram_payer, [&](auto &i) {
          i.is_active = FALSE;
          i.balance = get_balance(name(itr->account));
          i.updated_at = now();
        });
      }
    }
  }

  //get min paid creditor balance
  uint64_t get_min_paid_creditor_balance()
  {

    uint64_t balance = 10000 * 10000; // 10000 EOS
    plan_table p(code_account, code_account.value);
    eosio_assert(p.begin() != p.end(), "plan table is empty!");
    auto itr = p.begin();
    while (itr != p.end())
    {
      auto required = itr->cpu.amount + itr->net.amount;
      if (itr->is_free == false && itr->is_active && required < balance) {
        balance = required;
      }
      itr++;
    }
    return balance;
  }

  //check creditor enabled safedelegate or not
  bool is_safe_creditor(name creditor)
  {
    safecreditor_table s(code_account, SCOPE_CREDITOR>>1);
    auto itr = s.find(creditor.value);
    if(itr == s.end()){
      return false;
    } else {
      return true;
    }
  }

  //rotate active creditor
  void rotate_creditor()
  {
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto free_creditor = get_active_creditor(TRUE);
    auto paid_creditor = get_active_creditor(FALSE);

    asset free_balance = get_balance(free_creditor);
    asset paid_balance = get_balance(paid_creditor);
    uint64_t min_paid_creditor_balance = get_min_paid_creditor_balance();
    auto free_rotated = free_balance.amount > MIN_FREE_CREDITOR_BALANCE ?TRUE:FALSE;
    auto paid_rotated = paid_balance.amount > min_paid_creditor_balance ?TRUE:FALSE;
    auto idx = c.get_index<"updated.at"_n>();
    auto itr = idx.begin();
    while (itr != idx.end())
    {
      if(itr->for_free == TRUE)
      {
        if(free_rotated == TRUE){itr++;continue;}
        auto balance = get_balance(name(itr->account));
        if (name(itr->account) != free_creditor && balance.amount > MIN_FREE_CREDITOR_BALANCE)
        {
          activate_creditor(name(itr->account));
          free_rotated = TRUE;
        }
        itr++;
      }
      else
      {
        if(paid_rotated == TRUE){itr++;continue;}
        auto balance = get_balance(name(itr->account));
        if (name(itr->account) != paid_creditor && balance.amount > min_paid_creditor_balance)
        {
          activate_creditor(name(itr->account));
          paid_rotated = TRUE;
        }
        itr++;
      }
    }
  }
}
