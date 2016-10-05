#include <algorithm>
#include <cinttypes>
#include <iostream>

#include "deals.hpp"
#include "shared_memory.hpp"
#include "timing.hpp"
#include "top_destinations.hpp"

namespace top {
// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
TopDstDatabase::TopDstDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<i::DstInfo>(TOPDST_TABLENAME, TOPDST_PAGES /* pages */,
                                               TOPDST_ELEMENTS /* elements in page */,
                                               TOPDST_EXPIRES /* page expire */);
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
TopDstDatabase::~TopDstDatabase() {
  delete db_index;
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
void TopDstDatabase::truncate() {
  db_index->cleanup();
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
bool TopDstDatabase::addDestination(std::string locale, std::string destination,
                                    std::string departure_date) {
  uint32_t departure_date_int = query::date_to_int(departure_date);
  if (departure_date_int == 0) {
    std::cout << "addDestination() wrong departure date:" << departure_date << std::endl;
    return false;
  }

  i::DstInfo info;
  info.locale = query::locale_to_code(locale);
  info.destination = query::origin_to_code(destination);
  info.departure_date = departure_date_int;

  // Secondly add deal to index, include data position information
  shared_mem::ElementPointer<i::DstInfo> di_result = db_index->addRecord(&info);

  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR addDestination():" << (int)di_result.error << std::endl;
    return false;
  }

  return true;
}

std::vector<DstInfo> TopDstDatabase::getLocaleTop(std::string locale,
                                                  std::string departure_date_from,
                                                  std::string departure_date_to, uint16_t limit) {
  std::vector<DstInfo> result;

  TopDstSearchQuery query(*db_index);

  query.locale(locale);
  query.departure_dates(departure_date_from, departure_date_to);
  query.result_limit(limit);

  result = query.exec();

  return result;
}

std::vector<DstInfo> TopDstSearchQuery::exec() {
  grouped_destinations.clear();

  table.processRecords(*this);

  // convert result
  std::vector<DstInfo> top_destinations;

  for (auto& v : grouped_destinations) {
    top_destinations.push_back({v.first, v.second});
  }

  std::sort(top_destinations.begin(), top_destinations.end(),
            [](const DstInfo& a, const DstInfo& b) { return a.counter > b.counter; });

  if (top_destinations.size() > filter_limit) {
    top_destinations.resize(filter_limit);
  }

  // for (auto dst : top_destinations) { utils::print(dst); }
  return top_destinations;
}

/* function that will be called by TableProcessor
      *  for iterating over all not expired pages in table */
bool TopDstSearchQuery::process_function(i::DstInfo* elements, uint32_t size) {
  for (uint32_t idx = 0; idx < size; ++idx) {
    const i::DstInfo& current_element = elements[idx];

    // ******************************************************************
    // FILTERING OUT AREA
    // ******************************************************************

    // if departure date interval provided let's look it matches
    // --------------------------------
    if (filter_locale) {
      if (locale_value != current_element.locale) {
        // std::cout << "filter_locale" << std::endl;
        continue;
      }
    }

    // if departure date interval provided let's look it matches
    // --------------------------------
    if (filter_departure_date) {
      if (current_element.departure_date < departure_date_values.from ||
          current_element.departure_date > departure_date_values.to) {
        // std::cout << "filter_departure_date" << std::endl;
        continue;
      }
    }

    // **********************************************************************
    // BUILD DESTINATIONS HASH AREA
    // **********************************************************************
  }

  // true - means continue to iterate
  return true;
}

namespace utils {
void print(const i::DstInfo& deal) {
  std::cout << "i::DEAL: " << query::code_to_locale(deal.locale) << " "
            << query::code_to_origin(deal.destination) << " "
            << query::int_to_date(deal.departure_date) << std::endl;
}

void print(const DstInfo& deal) {
  std::cout << "DEAL: " << query::code_to_origin(deal.destination) << " " << deal.counter
            << std::endl;
}
}  // i namespace

void unit_test() {
  std::cout << "NO TEST YET" << std::endl;
}
}  // top namespace
