#include "deals_cheapest_by_country.hpp"

namespace deals {
//---------------------------------------------------------
void CheapestByCountry::pre_search() {
  grouped_max_price = 0;
}

//---------------------------------------------------------
void CheapestByCountry::process_deal(const i::DealInfo &deal) {
  if (grouped_by_country.size() > result_destinations_count) {
    if (grouped_max_price <= deal.price) {
      return;  // deal price is far more expensive, skip grouping
    }
  }
  if (grouped_max_price < deal.price) {
    grouped_max_price = deal.price;
  }

  auto &dst_deal = grouped_by_country[deal.destination_country];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
  }
  // if  not cheaper but same dates, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct) {
    dst_deal = deal;
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByCountry::post_search() {
  for (const auto &deal : grouped_by_country) {
    exec_result.push_back(deal.second);
  }

  // sort by departure_date ASC
  std::sort(exec_result.begin(), exec_result.end(), [](const i::DealInfo &a, const i::DealInfo &b) {
    return a.destination_country < b.destination_country;
  });
}

//----------------------------------------------------------------
std::vector<i::DealInfo> CheapestByCountry::get_result() {
  return exec_result;
}
}  // namespace deals