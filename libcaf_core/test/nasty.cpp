#include "nasty.hpp"

std::string to_string(weekday x) {
  switch (x) {
    default:
      return "???";
    case weekday::monday:
      return "monday";
    case weekday::tuesday:
      return "tuesday";
    case weekday::wednesday:
      return "wednesday";
    case weekday::thursday:
      return "thursday";
    case weekday::friday:
      return "friday";
    case weekday::saturday:
      return "saturday";
    case weekday::sunday:
      return "sunday";
  }
}

bool parse(caf::string_view input, weekday& dest) {
  if (input == "monday") {
    dest = weekday::monday;
    return true;
  }
  if (input == "tuesday") {
    dest = weekday::tuesday;
    return true;
  }
  if (input == "wednesday") {
    dest = weekday::wednesday;
    return true;
  }
  if (input == "thursday") {
    dest = weekday::thursday;
    return true;
  }
  if (input == "friday") {
    dest = weekday::friday;
    return true;
  }
  if (input == "saturday") {
    dest = weekday::saturday;
    return true;
  }
  if (input == "sunday") {
    dest = weekday::sunday;
    return true;
  }
  return false;
}
