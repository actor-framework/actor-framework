/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE affinity_parser
#include "caf/test/unit_test.hpp"

#include <set>
#include <string>
#include <vector>

#include "caf/affinity/affinity_parser.hpp"

using std::string;
using corelist = std::vector<std::set<int>>;
using namespace caf;

CAF_TEST(only groups) {
  string aff_str;
  corelist correct, res;

  aff_str = "<1>";
  correct = {{1}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1><2>";
  correct = {{1}, {2}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1><2><3>";
  correct = {{1}, {2}, {3}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1><2><3><4>";
  correct = {{1}, {2}, {3}, {4}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "  <  1  >  <    2 >  < 3 > < 4   >   ";
  correct = {{1}, {2}, {3}, {4}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(only sets) {
  string aff_str;
  corelist correct, res;

  aff_str = "<1,2>";
  correct = {{1, 2}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1,2,3>";
  correct = {{1, 2, 3}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1,2,3,4>";
  correct = {{1, 2, 3, 4}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1,2,3,4,5>";
  correct = {{1, 2, 3, 4, 5}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "   < 1 ,    2 , 3 , 4 ,     5  >             ";
  correct = {{1, 2, 3, 4, 5}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(only range) {
  string aff_str;
  corelist correct, res;

  aff_str = "<1-1>";
  correct = {{1}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-2>";
  correct = {{1, 2}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-3>";
  correct = {{1, 2, 3}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-1,2-3>";
  correct = {{1, 2, 3}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-1,2-3,4-6>";
  correct = {{1, 2, 3, 4, 5, 6}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-1,2-3,4-6,7-10>";
  correct = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " <   1 -    1 ,2-    3 , 4   -6 , 7  - 10       >      ";
  correct = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(empty group) {
  string aff_str;
  corelist correct, res;

  aff_str = "<>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<2,5><>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< 2, 5> <   >  ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< > <  2  , 3>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "  <    >  <> <  >  ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(not closed group) {
  string aff_str;
  corelist correct, res;

  aff_str = "<";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "    <    ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "  < 1- 2, 3";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< 1,  3> <   ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< 1,  3> <5  <5-9>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = ">";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "   >        ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " 1  - 2, 2   >";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " <5>1,  3>     ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " <5> 1,3><4-7>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(wrong value) {
  string aff_str;
  corelist correct, res;

  aff_str = "a sd < k >lj< ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< ddd > ,-  ndjz< ks";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<4,5 ,, 3>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<4,5 , - , 3>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<4,5 , -3 , 3>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<-- -5 ->";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}

CAF_TEST(bonus) {
  string aff_str;
  corelist correct, res;

  aff_str = " <A + B 0> <C1>	<2> < 3> <4  >   <5>< 6> <7>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1 <> <1-5>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "  <5,4,	 3,, 2, 1  ,   0, 6, 7 ,-,>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< 0 -  23 > ";
  correct = {{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<0-5, 6-	11> <>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " <0	-5>  <6 - 11> 	<18-23, 12 , 13> ";
  correct = {{0,1,2,3,4,5},{6,7,8,9,10,11},{12,13,18,19,20,21,22,23}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1,	 71, 9>";
  correct = {{1,9,71}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " 0-16	  , 	   1 - 17";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = " 0-16 	1-17";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "0-12 24-35, 13-23 36-47 ";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "";
  correct = {};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "-";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "-";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
  
  aff_str = "1-";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "< <1>-";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<100-102-104>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<100-102 104>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "   <100 -102  ,104>";
  correct = {{100,101,102,104}};
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);

  aff_str = "<1-10>< -11>< ,-12>		    <->    <13, 41><---45, 88- -98>";
  correct = {}; // error
  affinity::parser::parseaffinity(aff_str, res);
  CAF_CHECK_EQUAL(correct, res);
}
