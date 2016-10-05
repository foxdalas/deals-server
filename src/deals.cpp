#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals.hpp"
#include "timing.hpp"

namespace deals {
//      ***************************************************
//                   Deals search base class
//      ***************************************************

/* ----------------------------------------------------------
**  DealsSearchQuery execute()    execute search process
** ----------------------------------------------------------*/
void DealsSearchQuery::execute() {
  // if there was bad parameters -> no processing required
  if (query_is_broken) {
    std::cout << "ERROR: query has inconsistent parameters" << std::endl;
    throw "something wrong with request parameters";
    return;
  }

  // define destinations count we will look for
  // -------------------------------------------
  if (filter_destination) {
    result_destinations_count = destination_values_set.size();
  } else {
    result_destinations_count = filter_limit;
  }

  // run presearch in child class context
  pre_search();

  // table processor iterates table pages and call DealsSearchQuery::process_function()
  table.processRecords(*this);

  // run postsearch in child class context
  post_search();
};

//----------------------------------------------------------------
// DealsSearchQuery process_function()
// function that will be called by TableProcessor
// for iterating over all not expired pages in table
//----------------------------------------------------------------
bool DealsSearchQuery::process_function(i::DealInfo *elements, uint32_t size) {
  // skip whole page if expired
  if (filter_timestamp) {
    const i::DealInfo &lastdeal = elements[size - 1];
    if (timestamp_value > lastdeal.timestamp) {
      // std::cout << "whole page is expired" << std::endl;
      return true;
    }
  }

  // go throught all deals and aplly filters
  // if all filters are good -> process deal
  for (uint32_t idx = 0; idx < size; ++idx) {
    const i::DealInfo &deal = elements[idx];

    // filter_origin
    // --------------------------------
    if (filter_origin && origin_value != deal.origin) {
      // std::cout << "filter_origin" << std::endl;
      continue;
    }

    // filter_timestamp
    //--------------------------------
    if (filter_timestamp && timestamp_value > deal.timestamp) {
      // std::cout << "filter_timestamp" << std::endl;
      continue;
    }

    // filter_flight_by_roundtrip
    // --------------------------------
    if (filter_flight_by_roundtrip) {
      if (roundtrip_flight_flag == true) {
        if (deal.return_date == 0) {
          // std::cout << "filter_flight_by_roundtrip (this is ow)" << roundtrip_flight_flag << " "
          // << deal.return_date << std::endl;
          continue;
        }
      } else {
        if (deal.return_date != 0) {
          // std::cout << "filter_flight_by_roundtrip (this is rt)" << roundtrip_flight_flag << " "
          // << deal.return_date << std::endl;
          continue;
        }
      }
    }

    // filter_destination
    // --------------------------------
    if (filter_destination) {
      auto search = destination_values_set.find(deal.destination);
      if (search == destination_values_set.end()) {
        // std::cout << "filter_destination not found" << std::endl;
        continue;
      }
    }

    // filter_departure_date
    // --------------------------------
    if (filter_departure_date) {
      if (deal.departure_date < departure_date_values.from ||
          deal.departure_date > departure_date_values.to) {
        // std::cout << "filter_departure_date" << std::endl;
        continue;
      }
    }

    // filter_return_date
    // --------------------------------
    if (filter_return_date) {
      if (deal.return_date < return_date_values.from || deal.return_date > return_date_values.to) {
        // std::cout << "filter_return_date" << return_date_values.from << " " <<
        // return_date_values.to << std::endl;
        continue;
      }
    }

    // filter_stay_days
    //------------------------------------
    if (filter_stay_days && deal.return_date) {
      if (deal.stay_days < stay_days_values.from || deal.stay_days > stay_days_values.to) {
        // std::cout << "filter_stay_days" << std::endl;
        continue;
      }
    }

    // filter_flight_by_stops
    // --------------------------------
    if (filter_flight_by_stops) {
      if (direct_flights_flag != deal.flags.direct) {
        // std::cout << "filter_flight_by_stops" << std::endl;
        continue;
      }
    }

    // filter_price
    // ----------------------------------------
    if (filter_price) {
      if (deal.price < price_from_value || deal.price > price_to_value) {
        // std::cout << "filter by price:" << deal.price << std::endl;
        continue;
      }
    }

    // filter_departure_weekdays
    //------------------------------------
    if (filter_departure_weekdays) {
      if (((1 << deal.flags.departure_day_of_week) & departure_weekdays_bitmask) == 0) {
        // std::cout << "filter_departure_weekdays" << std::endl;
        continue;
      }
    }

    // filter_return_weekdays
    //------------------------------------
    if (filter_return_weekdays && deal.return_date) {
      if (((1 << deal.flags.return_day_of_week) & return_weekdays_bitmask) == 0) {
        // std::cout << "filter_return_weekdays" << std::endl;
        continue;
      }
    }

    // **********************************************************************
    // Deal match for all selected filters -> process it @ derivered class
    // **********************************************************************
    if (!process_deal(deal)) {
      // stop here if function return false
      return false;
    }
  }  // end of page elements iteration loop

  // go on with next table page
  // true - means continue to iterate
  return true;
}

//------------------------------------------------------------------------------
//      ***************************************************
//                   Deals Database class
//      ***************************************************
DealsDatabase::DealsDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<i::DealInfo>(DEALINFO_TABLENAME, DEALINFO_PAGES /* pages */,
                                                DEALINFO_ELEMENTS /* elements in page */,
                                                DEALS_EXPIRES /* page expire */);

  // 10k pages x 3.2m per page = 32g bytes, expire 60 seconds
  db_data = new shared_mem::Table<i::DealData>(DEALDATA_TABLENAME, DEALDATA_PAGES /* pages */,
                                               DEALDATA_ELEMENTS /* elements in page */,
                                               DEALS_EXPIRES /* page expire */);
}

//---------------------------------------------------------
// DealsDatabase destructor
//---------------------------------------------------------
DealsDatabase::~DealsDatabase() {
  delete db_data;
  delete db_index;
}

//---------------------------------------------------------
//  DealsDatabase  truncate
//---------------------------------------------------------
void DealsDatabase::truncate() {
  db_data->cleanup();
  db_index->cleanup();
}

//---------------------------------------------------------
//  DealsDatabase  addDeal
//---------------------------------------------------------
bool DealsDatabase::addDeal(std::string origin, std::string destination, std::string departure_date,
                            std::string return_date, bool direct_flight, uint32_t price,
                            std::string data) {
  if (origin.length() != 3) {
    std::cout << "wrong origin length:" << origin << std::endl;
    return false;
  }

  if (destination.length() != 3) {
    std::cout << "wrong destination length:" << destination << std::endl;
    return false;
  }

  uint32_t departure_date_int = query::date_to_int(departure_date);
  if (departure_date_int == 0) {
    std::cout << "wrong departure date:" << departure_date << std::endl;
    return false;
  }

  uint32_t return_date_int = query::date_to_int(return_date);

  // convert string to i::DealData (byte array)
  deals::i::DealData *data_pointer = (deals::i::DealData *)data.c_str();
  uint32_t data_size = data.length();

  // 1) Add data and get data offset in db page
  shared_mem::ElementPointer<i::DealData> result = db_data->addRecord(data_pointer, data_size);
  if (result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR DealsDatabase::addDeal 1:" << (int)result.error << std::endl;
    return false;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;

  i::DealInfo info;
  info.timestamp = timing::getTimestampSec();
  info.origin = query::origin_to_code(origin);
  info.destination = query::origin_to_code(destination);
  info.departure_date = departure_date_int;
  info.return_date = return_date_int;
  info.flags.overriden = false;
  info.flags.direct = direct_flight;
  info.flags.departure_day_of_week = ::utils::day_of_week_from_date(departure_date);
  info.flags.return_day_of_week = ::utils::day_of_week_from_date(return_date);
  info.price = price;
  strncpy(info.page_name, result.page_name.c_str(), MEMPAGE_NAME_MAX_LEN);
  info.index = result.index;
  info.size = result.size;

  if (return_date_int) {
    uint32_t days = ::utils::days_between_dates(departure_date, return_date);
    info.stay_days = days > UINT8_MAX ? UINT8_MAX : days;
  } else {
    info.stay_days = UINT8_MAX;
  }

  // 2) Add deal to index, with data position information
  shared_mem::ElementPointer<i::DealInfo> di_result = db_index->addRecord(&info);
  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR DealsDatabase::addDeal 2:" << (int)di_result.error << std::endl;
    return false;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;
  // std::cout << "addDeal OK" << std::endl;
  return true;
}

/*---------------------------------------------------------
* DealsDatabase  fill_deals_with_data
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::fill_deals_with_data(std::vector<i::DealInfo> i_deals) {
  // internal <i::DealInfo> contain shared memory page name and
  // information offsets. It's not useful anywhere outside
  // Let's transform internal format to external <DealInfo>
  std::vector<DealInfo> result;

  for (auto &deal : i_deals) {
    shared_mem::ElementPointer<i::DealData> deal_data = {*db_data, deal.page_name, deal.index,
                                                         deal.size};
    i::DealData *data_pointer = deal_data.get_data();
    std::string data = {(char *)data_pointer, deal.size};

    result.push_back((DealInfo){
        deal.timestamp, query::code_to_origin(deal.origin), query::code_to_origin(deal.destination),
        query::int_to_date(deal.departure_date), query::int_to_date(deal.return_date),
        deal.stay_days, deal.flags, deal.price, data});
  }

  return result;
}

//---------------------------------------------------------------------------------------
//          CHEAPEST BY DATES (simple std::map version)  ********************************
//---------------------------------------------------------------------------------------

/*---------------------------------------------------------
* DealsDatabase  searchForCheapest
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapest(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights)

{
  DealsCheapestByDatesSimple2 query(*db_index);  // <- table processed by search class

  // short for of applying all filters
  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  // load deals data from data pages (DealData shared memory pagers)
  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);

  return result;
}

//----------------------------------------------------------------
// DealsCheapestByDatesSimple PRESEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple::pre_search() {
  // nothing to do here
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestByDatesSimple::process_deal(const i::DealInfo &deal) {
  auto &dst_deal = grouped_destinations[deal.destination];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
  }
  // if  not cheaper but same dates and direct/stops, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.flags.direct == dst_deal.flags.direct) {
    dst_deal = deal;
    dst_deal.flags.overriden = true;
  }

  return true;
}

//----------------------------------------------------------------
// DealsCheapestByDatesSimple POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple::post_search() {
  // process results
  for (auto &v : grouped_destinations) {
    exec_result.push_back(v.second);
  }

  // sort resuilts by price ASC
  std::sort(exec_result.begin(), exec_result.end(),
            [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });

  if (exec_result.size() > result_destinations_count) {
    exec_result.resize(result_destinations_count);
  }

  // reduce output size
  if (filter_limit && exec_result.size() > filter_limit) {
    exec_result.resize(filter_limit);
  }
}

//---------------------------------------------------------------------------------------
//      CHEAPEST BY DATES (simple std::map with pointer version) ************************
//---------------------------------------------------------------------------------------
//----------------------------------------------------------------
// DealsCheapestByDatesSimple2 PRESEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple2::pre_search() {
  // nothing to do here
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestByDatesSimple2::process_deal(const i::DealInfo &deal) {
  auto elm = grouped_destinations.find(deal.destination);

  if (elm == grouped_destinations.end()) {
    grouped_destinations[deal.destination] = (i::DealInfo *)&deal;
    return true;
  }

  auto dst_deal = elm->second;
  if (dst_deal->price >= deal.price) {
    grouped_destinations[deal.destination] = (i::DealInfo *)&deal;
  }
  // if  not cheaper but same dates and direct/stops, replace with newer results
  else if (deal.departure_date == dst_deal->departure_date &&
           deal.return_date == dst_deal->return_date &&
           deal.flags.direct == dst_deal->flags.direct) {
    grouped_destinations[deal.destination] = (i::DealInfo *)&deal;
    // elm->second->flags.overriden = true;
  }

  return true;
}

//----------------------------------------------------------------
// DealsCheapestByDatesSimple2 POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple2::post_search() {
  // process results
  for (auto &v : grouped_destinations) {
    exec_result.push_back(*v.second);
  }

  // sort resuilts by price ASC
  std::sort(exec_result.begin(), exec_result.end(),
            [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });

  if (exec_result.size() > result_destinations_count) {
    exec_result.resize(result_destinations_count);
  }

  // reduce output size
  if (filter_limit && exec_result.size() > filter_limit) {
    exec_result.resize(filter_limit);
  }
}

//---------------------------------------------------------------------------------------
//                   CHEAPEST DAY BY DAY (2nd version)   ********************************
//---------------------------------------------------------------------------------------

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestDayByDay
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestDayByDay(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights) {
  //
  DealsCheapestDayByDay query(*db_index);

  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);
  // for (auto& deal : result) { deals::utils::print(deal); }
  return result;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay PRESEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::pre_search() {
  // filter_departure_date must be specified
  if (!filter_departure_date || !departure_date_values.duration) {
    std::cout << "ERROR no departure_date range" << std::endl;
    throw "departure dates interval must be specified";
  }

  // 3 city * 365 days - limit
  if (result_destinations_count * departure_date_values.duration > 1098) {
    std::cout << "ERROR result_destinations_count * departure_date_values.duration > 1098"
              << std::endl;
    throw "too much deals count requested, reduce destinations or dates range";
  }
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestDayByDay::process_deal(const i::DealInfo &deal) {
  auto &dst_dates = grouped_destinations_and_dates[deal.destination];
  auto &dst_deal = dst_dates[deal.departure_date];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
  }
  // if  not cheaper but same dates, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.flags.direct == dst_deal.flags.direct) {
    dst_deal = deal;
    dst_deal.flags.overriden = true;
  }

  return true;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::post_search() {
  // process results
  for (auto &dates : grouped_destinations_and_dates) {
    for (auto &deal : dates.second) {
      exec_result.push_back(deal.second);
    }
  }

  // sort by date ASC
  std::sort(exec_result.begin(), exec_result.end(), [](const i::DealInfo &a, const i::DealInfo &b) {
    return a.departure_date < b.departure_date;
  });
}

//***********************************************************
//                   UTILS
//***********************************************************
namespace utils {
void print(const i::DealInfo &deal) {
  std::cout << "i::DEAL: (" << query::int_to_date(deal.departure_date) << ")"
            << query::code_to_origin(deal.origin) << "-" << query::code_to_origin(deal.destination)
            << "(" << query::int_to_date(deal.return_date) << ") : " << deal.price << " "
            << deal.page_name << ":" << deal.index << std::endl;
}
void print(const DealInfo &deal) {
  std::cout << "DEAL: (" << deal.departure_date << ")" << deal.origin << "-" << deal.destination
            << "(" << deal.return_date << ") : " << deal.price << std::endl;
}

std::string sprint(const DealInfo &deal) {
  return "(" + deal.departure_date + ")" + deal.origin + "-" + deal.destination + "(" +
         deal.return_date + ") : " + std::to_string(deal.price) + "|" + deal.data + "\n";
}

}  // utils namespace

//------------------------------------------------------------------------
// TESTING:
//------------------------------------------------------------------------
std::string getRandomOrigin() {
  static const std::string origins[] = {"MOW", "MAD", "BER", "LON", "PAR",
                                        "LAX", "LED", "FRA", "BAR"};
  uint16_t place = rand() % (sizeof(origins) / sizeof(origins[0]));
  return origins[place];
}

uint32_t getRandomPrice(uint32_t minPrice) {
  uint32_t price = rand() & 0x0000FFFF;
  price += minPrice;

  if (price < minPrice) {
    std::cout << "ALARM!! " << minPrice << " " << price << std::endl;
  }
  return price;
}

std::string getRandomDate(uint32_t year = 2016) {
  uint32_t month = (rand() & 0x00000003) + (rand() & 0x00000003) + (rand() & 0x00000003) + 1;
  uint32_t day = (rand() & 0x00000007) + (rand() & 0x00000007) + (rand() & 0x00000007) + 1;

  return query::int_to_date(year * 10000 + month * 100 + day);
}

void convertertionsTest() {
  std::cout << "Origin encoder/decoder" << std::endl;
  std::string origins[10] = {"MOW", "MAD", "BER", "PAR", "LON", "FRA", "VKO", "JFK", "LAX", "MEX"};

  for (int i = 0; i < 10; ++i) {
    uint32_t code = query::origin_to_code(origins[i]);
    std::string decode = query::code_to_origin(code);
    assert(origins[i] == decode);
  }

  std::cout << "Locale encoder/decoder" << std::endl;
  std::string locales[] = {"ru", "de", "uk", "ua", "us"};

  for (int i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i) {
    uint32_t code = query::locale_to_code(locales[i]);
    std::string decode = query::code_to_locale(code);
    assert(locales[i] == decode);
  }

  std::cout << "Date encoder/decoder\n";
  uint32_t code = query::date_to_int("2017-01-01");
  std::string date = query::int_to_date(code);

  assert(code == 20170101);
  assert(date == "2017-01-01");
}

#define TEST_ELEMENTS_COUNT 50000
void unit_test() {
  assert(::utils::days_between_dates("2015-01-01", "2015-01-01") == 0);
  assert(::utils::days_between_dates("2015-01-01", "2016-01-01") == 365);
  assert(::utils::days_between_dates("2015-02-28", "2015-03-01") == 1);

  assert(::utils::day_of_week_str_from_date("2016-06-25") == "sat");
  assert(::utils::day_of_week_str_from_date("2016-04-13") == "wed");
  assert(::utils::day_of_week_from_str("sat") == 5);
  assert(::utils::day_of_week_from_str("mon") == 0);
  assert(::utils::day_of_week_from_str("sun") == 6);
  assert(::utils::day_of_week_from_str("eff") == 7);
  std::cout << "Date functions... OK" << std::endl;

  convertertionsTest();
  std::cout << "City conv functions... OK" << std::endl;

  DealsDatabase db;
  db.truncate();

  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  std::string check = "7, 7, 7";

  timing::Timer timer("SimpleSearch");

  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(1000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(2000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(3000), dumb);
  }

  // go to the feature (+1000 seconds)
  timing::TimeLord time;
  time += 1000;

  // add data we will expect
  db.addDeal("MOW", "MAD", "2016-05-01", "2016-05-21", true, 5000, check);
  db.addDeal("MOW", "BER", "2016-06-01", "2016-06-11", true, 6000, check);
  db.addDeal("MOW", "PAR", "2016-07-01", "2016-07-15", true, 7000, check);

  time += 5;

  // add some good
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), "MAD", getRandomDate(2015), getRandomDate(2015), true,
               getRandomPrice(5100), dumb);
    db.addDeal(getRandomOrigin(), "BER", getRandomDate(), getRandomDate(), true,
               getRandomPrice(6200), dumb);
    db.addDeal(getRandomOrigin(), "PAR", getRandomDate(), getRandomDate(), true,
               getRandomPrice(7200), dumb);

    // MAD will be 2016 here: and > 8000 price
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(8000), dumb);
  }

  timer.tick("before test1");
  // 1st test ----------------------------
  // *********************************************************
  std::vector<DealInfo> result = db.searchForCheapest("MOW", "AAA,PAR,BER,MAD", "", "", "", "", "",
                                                      "", 0, 0, ::utils::Threelean::Undefined, 0, 0,
                                                      0, 10, ::utils::Threelean::Undefined);
  timer.tick("test1");

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  assert(result.size() == 3);
  int city_count[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    if (result[i].destination == "MAD") {
      city_count[0]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 5000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 5000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-05-01");
      assert(result[i].return_date == "2016-05-21");
    } else if (result[i].destination == "BER") {
      city_count[1]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-06-01");
      assert(result[i].return_date == "2016-06-11");
    } else if (result[i].destination == "PAR") {
      city_count[2]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 7000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 7000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-07-01");
      assert(result[i].return_date == "2016-07-15");
    }
  }

  timer.tick("before test2");
  // 2nd test -------------------------------
  // *********************************************************
  result = db.searchForCheapest("MOW", "AAA,PAR,BER,MAD", "2016-06-01", "2016-06-23", "",
                                "2016-06-10", "2016-06-22", "", 0, 0, ::utils::Threelean::Undefined,
                                0, 0, 0, 10, ::utils::Threelean::Undefined);

  timer.tick("test2");

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  assert(result.size() <= 3);
  int city_count2[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    assert(query::date_to_int(result[i].departure_date) >= query::date_to_int("2016-06-01"));
    assert(query::date_to_int(result[i].departure_date) <= query::date_to_int("2016-06-23"));
    assert(query::date_to_int(result[i].return_date) >= query::date_to_int("2016-06-10"));
    assert(query::date_to_int(result[i].return_date) <= query::date_to_int("2016-06-22"));

    if (result[i].destination == "MAD") {
      city_count2[0]++;
      // madrid in this dates only over 8000
      assert(result[i].price >= 8000);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");

    } else if (result[i].destination == "BER") {
      city_count2[1]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-06-01");
      assert(result[i].return_date == "2016-06-11");

    } else if (result[i].destination == "PAR") {
      city_count2[2]++;
      // Paris in this dates only over 7200
      assert(result[i].price >= 7200);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
    }
  }

  assert(city_count2[0] <= 1);
  assert(city_count2[1] == 1);
  assert(city_count2[2] <= 1);

  // std::string origin, std::string destinations, std::string departure_date_from,
  //       std::string departure_date_to, std::string departure_days_of_week,
  //       std::string return_date_from, std::string return_date_to, std::string
  //       return_days_of_week,
  //       uint16_t stay_from, uint16_t stay_to, bool direct_flights, bool stops_flights, uint16_t
  //       limit,
  //       uint32_t max_lifetime_sec);

  //--------------
  // 3rd test -------------------------------
  // *********************************************************
  timer.tick("before test3");
  result = db.searchForCheapest("MOW", "", "", "", "fri,sat,sun", "", "", "sat,sun,mon", 4, 18,
                                ::utils::Threelean::False, 9100, 19200, 0, 2000,
                                ::utils::Threelean::Undefined);
  timer.tick("test3");
  std::cout << "search 3 result size:" << result.size() << std::endl;

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  for (int i = 0; i < result.size(); ++i) {
    assert(result[i].price >= 9100);
    assert(result[i].price <= 19200);
    assert(result[i].stay_days >= 4 && result[i].stay_days <= 18);
    assert(result[i].flags.direct == false);
    std::string dw = ::utils::day_of_week_str_from_code(result[i].flags.departure_day_of_week);
    std::string rw = ::utils::day_of_week_str_from_code(result[i].flags.return_day_of_week);
    assert(dw == "fri" || dw == "sat" || dw == "sun");
    assert(rw == "sat" || rw == "sun" || rw == "mon");
  }

  std::cout << "OK" << std::endl;
}

}  // deals namespace
