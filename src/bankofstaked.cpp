#include <eosiolib/transaction.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/eosio.system.hpp>
#include "../include/bankofstaked/bankofstaked.hpp"
#include "lock.cpp"
#include "utils.cpp"
#include "validation.cpp"
#include "safedelegatebw.cpp"

using namespace eosio;
using namespace eosiosystem;
using namespace bank;
using namespace lock;
using namespace utils;
using namespace validation;

class [[eosio::contract]] bankofstaked : contract
{

public:
  using contract::contract;

  [[eosio::action]]
  void clearhistory(uint64_t max_depth)
  {
    require_auth(code_account);
    uint64_t depth = 0;
    history_table o(code_account, SCOPE_ORDER>>1);
    while (o.begin() != o.end())
    {
      depth += 1;
      if(depth > max_depth) {
        break;
      }
      auto itr = o.end();
      itr--;
      o.erase(itr);
      history_table o(code_account, SCOPE_ORDER>>1);
    }
  }

  // DEBUG only, action to empty entires in both tables
  [[eosio::action]]
  void empty()
  {
    require_auth(code_account);
    /*
    plan_table p(code_account, code_account);
    while (p.begin() != p.end())
    {
      auto itr = p.end();
      itr--;
      p.erase(itr);
      plan_table p(code_account, code_account);
    }
    order_table o(code_account, SCOPE_ORDER>>1);
    while (o.begin() != o.end())
    {
      auto itr = o.end();
      itr--;
      o.erase(itr);
      order_table o(code_account, SCOPE_ORDER>>1);
    }

    plan_table p(code_account, code_account);
    while (p.begin() != p.end())
    {
      auto itr = p.end();
      itr--;
      p.erase(itr);
      plan_table p(code_account, code_account);
    }

    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    while (c.begin() != c.end())
    {
      auto itr = c.end();
      itr--;
      c.erase(itr);
      creditor_table c(code_account, SCOPE_CREDITOR>>1);
    }
    freelock_table c(code_account, SCOPE_FREELOCK>>1);
    while (c.begin() != c.end())
    {
      auto itr = c.end();
      itr--;
      c.erase(itr);
      freelock_table c(code_account, SCOPE_FREELOCK>>1);
    }
    */
  }


  [[eosio::action]]
  void check(name creditor)
  {
    require_auth(code_account);

    validate_creditor(creditor);

    order_table o(code_account, SCOPE_ORDER>>1);
    uint64_t depth = 0;
    std::vector<uint64_t> order_ids;

    // order ordered by expireat
    auto idx = o.get_index<"expireat"_n>();
    auto itr = idx.begin();
    //force expire at most CHECK_MAX_DEPTH orders
    while (itr != idx.end() && depth < CHECK_MAX_DEPTH)
    {
      if(now() >= itr->expireat) {
        order_ids.emplace_back(itr->id);
      }
      depth++;
      itr++;
    }
    undelegate(order_ids, 0);
    expire_freelock();
    rotate_creditor();
    get_balance(creditor);
  }

  [[eosio::action]]
  void forcexpire(const std::vector<uint64_t>& order_ids=std::vector<uint64_t>())
  {
    require_auth(code_account);

    //force expire provided orders
    undelegate(order_ids, 0);
    expire_freelock();
    rotate_creditor();
  }

  [[eosio::action]]
  void expireorder(uint64_t id)
  {
    require_auth(code_account);

    std::string content = "";
    order_table o(code_account, SCOPE_ORDER>>1);
    auto order = o.find(id);
    eosio_assert(order != o.end(), "order entry not found!!!");

    //save order meta to history
    //buyer|creditor|beneficiary|plan_id|price|cpu|net|createdat|expireat
    content += (name{order->buyer}).to_string();
    content += "|" + (name{order->creditor}).to_string();
    content += "|" + (name{order->beneficiary}).to_string();
    content += "|" + std::to_string(order->plan_id);
    content += "|" + std::to_string(order->price.amount);
    content += order->is_free==TRUE?"|free":"|paid";
    content += "|" + std::to_string(order->cpu_staked.amount);
    content += "|" + std::to_string(order->net_staked.amount);
    content += "|" + std::to_string(order->createdat);
    content += "|" + std::to_string(order->expireat);

    // updated cpu_staked/net_staked/cpu_unstaked/net_unstaked of creditor entry
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto creditor_itr = c.find(order->creditor);
    asset balance = get_balance(name(order->creditor));
    c.modify(creditor_itr, ram_payer, [&](auto &i) {
      i.cpu_staked -= order->cpu_staked;
      i.net_staked -= order->net_staked;
      i.cpu_unstaked += order->cpu_staked;
      i.net_unstaked += order->net_staked;
      i.balance = balance;
      i.updatedat = now();
    });

    //delete order entry
    o.erase(order);

    // save order mete data to history table
    history_table h(code_account, SCOPE_HISTORY>>1);
    h.emplace(ram_payer, [&](auto &i) {
      i.id = h.available_primary_key();
      i.content = content;
      i.createdat = now();
    });
  }

  [[eosio::action]]
  void addwhitelist(name account, uint64_t capacity)
  {
    require_auth(code_account);
    whitelist_table w(code_account, SCOPE_WHITELIST>>1);
    auto itr = w.find(account.value);
    if(itr == w.end()) {
      w.emplace(ram_payer, [&](auto &i) {
        i.account = account.value;
        i.capacity = capacity;
        i.createdat = now();
        i.updatedat = now();
      });
    } else {
      w.modify(itr, ram_payer, [&](auto &i) {
        i.capacity = capacity;
        i.updatedat = now();
      });
    }
  }

  [[eosio::action]]
  void delwhitelist(name account, uint64_t capacity)
  {
    require_auth(code_account);
    whitelist_table w(code_account, SCOPE_WHITELIST>>1);
    auto itr = w.find(account.value);
    eosio_assert(itr != w.end(), "account not found in whitelist table");
    //delelete whitelist entry
    w.erase(itr);
  }

  [[eosio::action]]
  void addcreditor(name account, uint64_t for_free, std::string free_memo)
  {
    require_auth(code_account);
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto itr = c.find(account.value);
    eosio_assert(itr == c.end(), "account already exist in creditor table");

    asset balance = get_balance(account);
    c.emplace(ram_payer, [&](auto &i) {
      i.isactive = FALSE;
      i.for_free = for_free?TRUE:FALSE;
      i.free_memo = for_free?free_memo:"";
      i.account = account.value;
      i.balance = balance;
      i.createdat = now();
      i.updatedat = 0; // set to 0 for creditor auto rotation
    });
  }

  [[eosio::action]]
  void addsafeacnt(name account)
  {
    require_auth(code_account);

    validate_creditor(account);

    safecreditor_table s(code_account, SCOPE_CREDITOR>>1);
    s.emplace(ram_payer, [&](auto &i) {
      i.account = account.value;
      i.createdat = now();
      i.updatedat = now();
    });
  }

  [[eosio::action]]
  void delsafeacnt(name account)
  {
    require_auth(code_account);
    safecreditor_table s(code_account, SCOPE_CREDITOR>>1);
    auto itr = s.find(account.value);
    eosio_assert(itr != s.end(), "account does not exist in safecreditor table");
    s.erase(itr);
  }


  [[eosio::action]]
  void delcreditor(name account)
  {
    require_auth(code_account);
    creditor_table c(code_account, SCOPE_CREDITOR>>1);
    auto itr = c.find(account.value);
    eosio_assert(itr!= c.end(), "account not found in creditor table");
    eosio_assert(itr->isactive == FALSE, "cannot delete active creditor");
    //delelete creditor entry
    c.erase(itr);
  }


  [[eosio::action]]
  void addblacklist(name account)
  {
    require_auth(code_account);
    blacklist_table b(code_account, SCOPE_BLACKLIST>>1);
    auto itr = b.find(account.value);
    eosio_assert(itr == b.end(), "account already exist in blacklist table");

    // add entry
    b.emplace(ram_payer, [&](auto &i) {
      i.account = account.value;
      i.createdat = now();
    });
  }


  [[eosio::action]]
  void delblacklist(name account)
  {
    require_auth(code_account);
    blacklist_table b(code_account, SCOPE_BLACKLIST>>1);

    //make sure specified blacklist account exists
    auto itr = b.find(account.value);
    eosio_assert(itr!= b.end(), "account not found in blacklist table");
    //delelete entry
    b.erase(itr);
  }


  [[eosio::action]]
  void activate(name account)
  {
    require_auth(code_account);
    activate_creditor(account);
  }


  [[eosio::action]]
  void setplan(asset price,
               asset cpu,
               asset net,
               uint64_t duration,
               bool is_free)
  {
    require_auth(code_account);
    validate_asset(price, cpu, net);
    plan_table p(code_account, code_account.value);
    auto idx = p.get_index<"price"_n>();
    auto itr = idx.find(price.amount);
    if (itr == idx.end())
    {
      p.emplace(ram_payer, [&](auto &i) {
        i.id = p.available_primary_key();
        i.price = price;
        i.cpu = cpu;
        i.net = net;
        i.duration = duration;
        i.isactive = FALSE;
        i.is_free = is_free?TRUE:FALSE;
        i.createdat = now();
        i.updatedat = now();
      });
    }
    else
    {
      idx.modify(itr, ram_payer, [&](auto &i) {
        i.cpu = cpu;
        i.net = net;
        i.duration = duration;
        i.is_free = is_free?TRUE:FALSE;
        i.updatedat = now();
      });
    }
  }
  
  [[eosio::action]]
  void activateplan(asset price, bool isactive)
  {
    require_auth(code_account);
    eosio_assert(price.is_valid(), "invalid price");
    plan_table p(code_account, code_account.value);
    auto idx = p.get_index<"price"_n>();
    auto itr = idx.find(price.amount);
    eosio_assert(itr != idx.end(), "price not found");

    idx.modify(itr, ram_payer, [&](auto &i) {
     i.isactive = isactive?TRUE:FALSE;
     i.updatedat = now();
    });
  }

  //token received
  void received_token(name from, name to, asset quantity, string memo)
  {
    //validation token transfer, only accept EOS transfer
    //eosio_assert(t.quantity.symbol==symbol_type(system_token_symbol), "only accept EOS transfer");
    eosio_assert(quantity.symbol.code() == symbol_code("EOS"), "only accept EOS transfer");

    if (to == _self)
    {
      name buyer = from;
      //if token comes from fundstostake, do nothing, just take it :)
      if (from == "fundstostake"_n)
      {
        return;
      }
      //validate plan, isactive should be TRUE
      plan_table p(code_account, code_account.value);
      auto idx = p.get_index<"price"_n>();
      auto plan = idx.find(quantity.amount);
      eosio_assert(plan->isactive == TRUE, "plan is in-active");
      eosio_assert(plan != idx.end(), "invalid price");

      name beneficiary = get_beneficiary(memo, buyer);
      name eosio = name("eosio");

      // if plan is free, validate there is no Freelock for this beneficiary
      if(plan->is_free == TRUE)
      {
        validate_freelock(beneficiary);
      }

      //get active creditor
      name creditor = get_active_creditor(plan->is_free);

      //if plan is not free, make sure creditor has enough balance to delegate
      if(plan->is_free == FALSE)
      {
          asset to_delegate = plan->cpu + plan->net;
          asset balance = get_balance(creditor);
          if(balance < to_delegate) {
            creditor = get_qualified_paid_creditor(to_delegate);
          }
      }

      //make sure creditor is a valid account
      eosio_assert( is_account( creditor ), "creditor account does not exist");

      //validate buyer
      //1. buyer shouldnt be code_account
      //2. buyer shouldnt be in blacklist
      //3. each buyer could only have 5 affective orders at most
      validate_buyer(buyer, plan->is_free);

      //validate beneficiary
      //1. beneficiary shouldnt be code_account
      //2. beneficiary shouldnt be in blacklist
      //3. each beneficiary could only have 5 affective orders at most
      validate_beneficiary(beneficiary, creditor, plan->is_free);

      //INLINE ACTION to delegate CPU&NET for beneficiary account
      if (is_safe_creditor(creditor)) {
        INLINE_ACTION_SENDER(safedelegatebw, delegatebw)
        (creditor, {{creditor, "creditorperm"_n}}, {beneficiary, plan->net, plan->cpu});
      } else {
        INLINE_ACTION_SENDER(eosiosystem::system_contract, delegatebw)
        (eosio, {{creditor, "creditorperm"_n}}, {creditor, beneficiary, plan->net, plan->cpu, false});
      }

      //INLINE ACTION to call check action of `bankofstaked`
      INLINE_ACTION_SENDER(bankofstaked, check)
      (code_account, {{code_account, "bankperm"_n}}, {creditor});

      // add cpu_staked&net_staked to creditor entry
      asset balance = get_balance(creditor);
      creditor_table c(code_account, SCOPE_CREDITOR>>1);
      auto creditor_itr = c.find(creditor.value);
      c.modify(creditor_itr, ram_payer, [&](auto &i) {
        i.cpu_staked += plan->cpu;
        i.net_staked += plan->net;
        i.balance = balance;
        i.updatedat = now();
      });

      //create Order entry
      uint64_t order_id;
      order_table o(code_account, SCOPE_ORDER>>1);
      o.emplace(ram_payer, [&](auto &i) {
        i.id = o.available_primary_key();
        i.buyer = buyer.value;
        i.price = plan->price;
        i.creditor = creditor.value;
        i.beneficiary = beneficiary.value;
        i.plan_id = plan->id;
        i.cpu_staked = plan->cpu;
        i.net_staked = plan->net;
        i.is_free = plan->is_free;
        i.createdat = now();
        i.expireat = now() + plan->duration * SECONDS_PER_MIN;

        order_id = i.id;
      });

      if(plan->is_free == TRUE)
      {
        // if plan is free, add a Freelock entry
        add_freelock(beneficiary);
        // auto refund immediately
        //INLINE ACTION to auto refund
        creditor_table c(code_account, SCOPE_CREDITOR>>1);
        std::string free_memo = c.get(creditor.value).free_memo;
        auto username = name{buyer};
        std::string buyer_name = username.to_string();
        std::string memo = buyer_name + " " + free_memo;
        INLINE_ACTION_SENDER(eosio::token, transfer)
        ("eosio.token"_n, {{code_account, "bankperm"_n}}, {code_account, safe_transfer_account, plan->price, memo});
      }

      //deferred transaction to auto undelegate after expired
      std::vector<uint64_t> order_ids;
      order_ids.emplace_back(order_id);
      undelegate(order_ids, plan->duration);
    }
  }

private:

  //undelegate Orders specified by order_ids
  //deferred(if duration > 0) transaction to auto undelegate after expired
  void undelegate(const std::vector<uint64_t>& order_ids=std::vector<uint64_t>(), uint64_t duration=0)
  {
    if(order_ids.size() == 0) 
    {
      return;
    }
    eosio::transaction out;

    order_table o(code_account, SCOPE_ORDER>>1);
    plan_table p(code_account, code_account.value);

    uint64_t nonce = 0;

    for(int i=0; i<order_ids.size(); i++)
    {
      uint64_t order_id = order_ids[i];
      nonce += order_id;
      // get order entry
      auto order = o.get(order_id);

      // undelegatebw action
      action act1 = action(
        permission_level{ name(order.creditor), "creditorperm"_n },
        "eosio"_n, "undelegatebw"_n,
        std::make_tuple(order.creditor, order.beneficiary, order.net_staked, order.cpu_staked)
      );
      out.actions.emplace_back(act1);
      //delete order entry
      action act2 = action(
        permission_level{ code_account, "bankperm"_n },
        code_account, "expireorder"_n,
        std::make_tuple(order_id)
      );
      out.actions.emplace_back(act2);

      //if order is_free is not free, transfer income to creditor
      if (order.is_free == FALSE)
      {
        auto plan = p.get(order.plan_id);

        auto username = name{order.creditor};
        std::string recipient_name = username.to_string();
        std::string memo = recipient_name + " bankofstaked income";

        // transfer income to creditor
        asset income = get_income(name(order.creditor), order.price);
        eosio_assert(income <= order.price, "income should not be greater than price");
        action act3 = action(
          permission_level{ code_account, "bankperm"_n },
          "eosio.token"_n, "transfer"_n,
          std::make_tuple(code_account, safe_transfer_account, income, memo)
        );
        out.actions.emplace_back(act3);

        // transfer reserved fund to reserved_account
        asset reserved = order.price - income;
        eosio_assert(reserved <= order.price, "reserved should not be greater than price");
        username = name{reserved_account};
        recipient_name = username.to_string();
        memo = recipient_name + " bankofstaked reserved";
        action act4 = action(
          permission_level{ code_account, "bankperm"_n },
          "eosio.token"_n, "transfer"_n,
          std::make_tuple(code_account, safe_transfer_account, reserved, memo)
        );
        out.actions.emplace_back(act4);

      }
    }

    if(duration > 0) {
      out.delay_sec = duration * SECONDS_PER_MIN;
    }
    out.send((uint128_t(code_account.value) << 64) | current_time() | nonce, code_account, true);
  }

};

extern "C" {
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (code == "eosio.token"_n.value && action == "transfer"_n.value) {
      eosio::execute_action(
        name(receiver), name(code), &bankofstaked::received_token
      );
    }

    if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(bankofstaked,
          (empty)
          (setplan)
          (activateplan)
          (expireorder)
          (addwhitelist)
          (delwhitelist)
          (addcreditor)
          (addsafeacnt)
          (delsafeacnt)
          (delcreditor)
          (addblacklist)
          (delblacklist)
          (activate)
          (check)
          (clearhistory)
          (forcexpire))
      }
    }
  }
}
