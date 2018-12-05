/**
 *  @file bankofstaked.hpp
 */
#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/multi_index.hpp>

#define EOS_SYMBOL S(4, EOS)

using namespace eosio;

namespace bank
{
static const name code_account = "bankofstaked"_n;
static const name ram_payer = "bankofstaked"_n;
static const name safe_transfer_account = "masktransfer"_n;
static const name reserved_account = "stakedincome"_n;
static const uint64_t SECONDS_PER_MIN = 60;
static const uint64_t SECONDS_PER_DAY = 24 * 3600;
static const uint64_t MAX_FREE_ORDERS = 5;
static const uint64_t MAX_PAID_ORDERS = 20;
static const uint64_t TRUE = 1;
static const uint64_t FALSE = 0;
static const uint64_t CHECK_MAX_DEPTH = 3;
static const uint64_t MAX_EOS_BALANCE = 500 * 10000; // 500 EOS at most
static const uint64_t MIN_FREE_CREDITOR_BALANCE = 10 * 10000; // 10 EOS at least
static const uint64_t DEFAULT_DIVIDENT_PERCENTAGE = 90; // 90% income will be allocated to creditor

// To protect your table, you can specify different scope as random numbers
static const uint64_t SCOPE_ORDER = 1842919517374;
static const uint64_t SCOPE_HISTORY = 1842919517374;
static const uint64_t SCOPE_CREDITOR = 1842919517374;
static const uint64_t SCOPE_FREELOCK = 1842919517374;
static const uint64_t SCOPE_BLACKLIST = 1842919517374;
static const uint64_t SCOPE_WHITELIST = 1842919517374;

// @abi table freelock i64
struct freelock
{
  uint64_t beneficiary; // account who received CPU&NET
  uint64_t createdat;      // unix time, in seconds
  uint64_t expireat;       // unix time, in seconds

  uint64_t primary_key() const { return beneficiary; }
  uint64_t get_expireat() const { return expireat; }

  EOSLIB_SERIALIZE(freelock, (beneficiary)(createdat)(expireat));
};

typedef multi_index<"freelock"_n, freelock,
                    indexed_by<"expireat"_n, const_mem_fun<freelock, uint64_t, &freelock::get_expireat>>>
    freelock_table;

// @abi table order i64
struct order
{
  uint64_t id;
  uint64_t buyer;
  asset price;              // amount of EOS paied
  uint64_t is_free;         // default is FALSE, for free plan, when service expired, it will do a auto refund
  uint64_t creditor;    // account who delegated CPU&NET
  uint64_t beneficiary; // account who received CPU&NET
  uint64_t plan_id;         // foreignkey of table plan
  asset cpu_staked;         // amount of EOS staked for cpu
  asset net_staked;         // amount of EOS staked for net
  uint64_t createdat;      // unix time, in seconds
  uint64_t expireat;       // unix time, in seconds

  auto primary_key() const { return id; }
  uint64_t get_buyer() const { return buyer; }
  uint64_t get_beneficiary() const { return beneficiary; }
  uint64_t get_expireat() const { return expireat; }

  EOSLIB_SERIALIZE(order, (id)(buyer)(price)(is_free)(creditor)(beneficiary)(plan_id)(cpu_staked)(net_staked)(createdat)(expireat));
};

typedef multi_index<"order"_n, order,
                    indexed_by<"buyer"_n, const_mem_fun<order, uint64_t, &order::get_buyer>>,
                    indexed_by<"expireat"_n, const_mem_fun<order, uint64_t, &order::get_expireat>>,
                    indexed_by<"beneficiary"_n, const_mem_fun<order, uint64_t, &order::get_beneficiary>>>
    order_table;

// @abi table history
struct history
{
  uint64_t id;
  string content;      // content
  uint64_t createdat; // unix time, in seconds

  auto primary_key() const { return id; }
  EOSLIB_SERIALIZE(history, (id)(content)(createdat));
};
typedef multi_index<"history"_n, history> history_table;

// @abi table plan i64
struct plan
{
  uint64_t id;
  asset price;         // amount of EOS paied
  asset cpu;           // amount of EOS staked for cpu
  asset net;           // amount of EOS staked for net
  uint64_t duration;   // affective time, in minutes
  uint64_t is_free;    // default is FALSE, for free plan, when service expired, it will do a auto refund
  uint64_t isactive;  // on active plan could be choosen
  uint64_t createdat; // unix time, in seconds
  uint64_t updatedat; // unix time, in seconds

  auto primary_key() const { return id; }
  uint64_t get_price() const { return (uint64_t)price.amount; }
  EOSLIB_SERIALIZE(plan, (id)(price)(cpu)(net)(duration)(is_free)(isactive)(createdat)(updatedat));
};
typedef multi_index<"plan"_n, plan,
                    indexed_by<"price"_n, const_mem_fun<plan, uint64_t, &plan::get_price>>>
    plan_table;

// @abi table safecreditor i64
struct safecreditor
{
  uint64_t account;
  uint64_t createdat; // unix time, in seconds
  uint64_t updatedat; // unix time, in seconds

  uint64_t primary_key() const { return account; }

  EOSLIB_SERIALIZE(safecreditor, (account)(createdat)(updatedat));
};
typedef multi_index<"safecreditor"_n, safecreditor> safecreditor_table;

// @abi table dividend i64
struct dividend 
{
  uint64_t account;
  uint64_t percentage; // percentage of income allocating to creditor

  uint64_t primary_key() const { return account; }

  EOSLIB_SERIALIZE(dividend, (account)(percentage));
};
typedef multi_index<"dividend"_n, dividend> dividend_table;

// @abi table creditor i64
struct creditor
{
  uint64_t account;
  uint64_t isactive;
  uint64_t for_free;         // default is FALSE, for_free means if this creditor provide free staking or not
  string free_memo;    // memo for refund transaction
  asset balance;              // amount of EOS paied
  asset cpu_staked;              // amount of EOS paied
  asset net_staked;              // amount of EOS paied
  asset cpu_unstaked;              // amount of EOS paied
  asset net_unstaked;              // amount of EOS paied
  uint64_t createdat; // unix time, in seconds
  uint64_t updatedat; // unix time, in seconds

  uint64_t primary_key() const { return account; }
  uint64_t get_isactive() const { return isactive; }
  uint64_t get_updatedat() const { return updatedat; }

  EOSLIB_SERIALIZE(creditor, (account)(isactive)(for_free)(free_memo)(balance)(cpu_staked)(net_staked)(cpu_unstaked)(net_unstaked)(createdat)(updatedat));
};

typedef multi_index<"creditor"_n, creditor,
                    indexed_by<"isactive"_n, const_mem_fun<creditor, uint64_t, &creditor::get_isactive>>,
                    indexed_by<"updatedat"_n, const_mem_fun<creditor, uint64_t, &creditor::get_updatedat>>>
    creditor_table;

// @abi table blacklist i64
struct blacklist
{
  uint64_t account;
  uint64_t createdat; // unix time, in seconds

  uint64_t primary_key() const { return account; }
  EOSLIB_SERIALIZE(blacklist, (account)(createdat));
};
typedef multi_index<"blacklist"_n, blacklist> blacklist_table;

// @abi table whitelist i64
struct whitelist
{
  uint64_t account;
  uint64_t capacity; // max in-use free orders
  uint64_t updatedat; // unix time, in seconds
  uint64_t createdat; // unix time, in seconds

  uint64_t primary_key() const { return account; }
  EOSLIB_SERIALIZE(whitelist, (account)(capacity)(updatedat)(createdat));
};
typedef multi_index<"whitelist"_n, whitelist> whitelist_table;

}// namespace bank


