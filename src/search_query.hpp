#ifndef SRC_SEARCH_QUERY_HPP
#define SRC_SEARCH_QUERY_HPP

#include "shared_memory.hpp"
#include "utils.hpp"

namespace query {

struct DateInterval {
  uint32_t from;
  uint32_t to;
};

struct StayInterval {
  uint8_t from;
  uint8_t to;
};

class SearchQuery {
 public:
  SearchQuery()
      : filter_origin(false),
        filter_destination(false),
        filter_departure_date(false),
        filter_return_date(false),
        filter_timestamp(false),
        filter_flight_by_stops(false),
        filter_departure_weekdays(false),
        filter_return_weekdays(false),
        filter_stay_days(false),
        filter_2gds4rt(false),
        filter_limit(20),
        query_is_broken(false),
        filter_price(false),
        filter_locale(false) {
  }

  void origin(std::string origin);
  void destinations(std::string destinations);
  void departure_dates(std::string departure_date_from, std::string departure_date_to);
  void return_dates(std::string return_date_from, std::string return_date_to);
  void direct_flights(bool direct_flights, bool stops_flights);
  void max_lifetime_sec(uint32_t max_lifetime);
  void result_limit(uint16_t limit);
  void stay_days(uint16_t stay_from, uint16_t stay_to);
  void departure_weekdays(std::string days_of_week);
  void return_weekdays(std::string days_of_week);
  void skip_2gds4rt(bool skip);
  void price(uint32_t price_from, uint32_t price_to);
  void locale(std::string locale);

 protected:
  uint8_t weekdays_bitmask(std::string days_of_week);

  bool filter_origin;
  uint32_t origin_value;

  bool filter_destination;
  std::vector<uint32_t> destination_values_vector;

  bool filter_departure_date;
  DateInterval departure_date_values;

  bool filter_return_date;
  DateInterval return_date_values;

  bool filter_timestamp;
  uint32_t timestamp_value;

  bool filter_flight_by_stops;
  bool direct_flights_flag;
  bool stops_flights_flag;

  bool filter_departure_weekdays;
  uint8_t departure_weekdays_bitmask;

  bool filter_return_weekdays;
  uint8_t return_weekdays_bitmask;

  bool filter_stay_days;
  StayInterval stay_days_values;

  bool filter_2gds4rt;

  uint16_t filter_limit;
  uint16_t deals_slots_used;
  uint16_t max_price_deal;
  bool query_is_broken;

  bool filter_price;
  uint32_t price_from_value;
  uint32_t price_to_value;

  bool filter_locale;
  uint16_t locale_value;
};

bool check_destinations_format(std::string destinations);
bool check_weekdays_format(std::string weekdays);
bool check_date_format(std::string date);
bool check_date_to_date(std::string date_from, std::string date_to);

union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);
uint32_t date_to_int(std::string date);
std::string int_to_date(uint32_t date);

union LocaleCodec {
  uint16_t int_code;
  char text_code[2];
};
uint16_t locale_to_code(std::string locale);
std::string code_to_locale(uint32_t code);

}  // namespace query

#endif