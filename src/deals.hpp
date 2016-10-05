#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include <map>
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

#define DEALS_EXPIRES 60 * 60 * 12

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 5000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 50000000

void unit_test();

struct Flags {
  bool direct : 1;
  bool overriden : 1;  // show that not cheapest but lastest in period
  uint8_t departure_day_of_week : 4;
  uint8_t return_day_of_week : 4;
};

namespace i {
struct DealInfo {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
  uint8_t stay_days;
  Flags flags;
  uint32_t price;
  char page_name[MEMPAGE_NAME_MAX_LEN];
  uint32_t index;
  uint32_t size;
};

using DealData = uint8_t;  // aka char
}  // namespace deals::i

struct DealInfo {
  uint32_t timestamp;
  std::string origin;
  std::string destination;
  std::string departure_date;
  std::string return_date;
  uint8_t stay_days;
  Flags flags;
  uint32_t price;
  std::string data;
};

namespace utils {
void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);
}  // namespace deals::utils

//------------------------------------------------------------
// DealsDatabase
//------------------------------------------------------------
class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  bool addDeal(std::string origin, std::string destination, std::string departure_date,
               std::string return_date, bool direct_flight, uint32_t price, std::string data);

  // find cheapest by selected filters
  std::vector<DealInfo> searchForCheapest(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from,
      uint32_t price_to, uint16_t limit, uint32_t max_lifetime_sec,
      ::utils::Threelean roundtrip_flights);

  // find cheapest for each day in provided deparutre interval
  std::vector<DealInfo> searchForCheapestDayByDay(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from,
      uint32_t price_to, uint16_t limit, uint32_t max_lifetime_sec,
      ::utils::Threelean roundtrip_flights);

  // clear database
  void truncate();

 private:
  std::vector<DealInfo> fill_deals_with_data(std::vector<i::DealInfo> i_deals);

  shared_mem::Table<i::DealInfo>* db_index;
  shared_mem::Table<i::DealData>* db_data;

  friend void unit_test();
};

//------------------------------------------------------------
// DealsSearchQuery
//------------------------------------------------------------
class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo>, public query::SearchQuery {
 protected:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table) : table(table) {
  }
  // preparations and actual processing
  void execute();

  // array size will be equal to filter_limit.
  // used for speed optimization, iteration throught vector is slower
  // uint32_t* destination_values = nullptr;  // <- array
  uint16_t result_destinations_count = 0;

 private:
  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table
  bool process_function(i::DealInfo* elements, uint32_t size) final override;

  // VIRTUAL FUNCTIONS SECTION:
  virtual bool process_deal(const i::DealInfo& deal) = 0;
  // if process_function() deside deals worth of processing
  // process_deal() will be called in parent class

  // before and after processing
  virtual void pre_search() = 0;
  virtual void post_search() = 0;

  shared_mem::Table<i::DealInfo>& table;
  friend class DealsDatabase;
};

//------------------------------------------------------------
// DealsCheapestByDatesSimple (Simple version of DealsCheapestByPeriod)
//------------------------------------------------------------
class DealsCheapestByDatesSimple : public DealsSearchQuery {
 public:
  DealsCheapestByDatesSimple(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  // implement virtual functions:
  bool process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;

  std::map<uint32_t, i::DealInfo> grouped_destinations;
  std::vector<i::DealInfo> exec_result;
};

//------------------------------------------------------------
// DealsCheapestByDatesSimple2 (version with pointers)
//------------------------------------------------------------
class DealsCheapestByDatesSimple2 : public DealsSearchQuery {
 public:
  DealsCheapestByDatesSimple2(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  // implement virtual functions:
  bool process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;

  std::map<uint32_t, i::DealInfo*> grouped_destinations;
  std::vector<i::DealInfo> exec_result;
};

//------------------------------------------------------------
// DealsCheapestDayByDay
//------------------------------------------------------------
class DealsCheapestDayByDay : public DealsSearchQuery {
 public:
  DealsCheapestDayByDay(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }

  // implement virtual functions:
  bool process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;

  std::map<uint32_t, std::map<uint32_t, i::DealInfo>> grouped_destinations_and_dates;
  std::vector<i::DealInfo> exec_result;
};

}  // namespace deals

#endif