#ifndef POSIX_TIME_TYPES_HPP___
#define POSIX_TIME_TYPES_HPP___
/* Copyright (c) 2002 CrystalClear Software, Inc.
 * Disclaimer & Full Copyright at end of file
 * Author: Jeff Garland 
 */

#include "boost/date_time/time_clock.hpp"
#include "boost/date_time/microsec_time_clock.hpp"
#include "boost/date_time/posix_time/ptime.hpp"
#include "boost/date_time/posix_time/posix_time_duration.hpp"
#include "boost/date_time/posix_time/posix_time_system.hpp"
#include "boost/date_time/posix_time/time_period.hpp"
#include "boost/date_time/time_iterator.hpp"
#include "boost/date_time/dst_rules.hpp"

namespace boost {

//!Defines a non-adjusted time system with nano-second resolution and stable calculation properties
namespace posix_time {

  //! Iterator over a defined time duration
  /*! \ingroup time_basics
   */
  typedef date_time::time_itr<ptime> time_iterator;
  //! A time clock that has a resolution of one second
  /*! \ingroup time_basics
   */
  typedef date_time::second_clock<ptime::date_type, ptime> second_clock;

#ifdef BOOST_DATE_TIME_HAS_GETTIMEOFDAY_HIGH_PRECISION_CLOCK
  //! A time clock that has a resolution of one microsecond
  /*! \ingroup time_basics
   */
  typedef date_time::microsec_clock<ptime> microsec_clock;
#endif

  //! Define a dst null dst rule for the posix_time system
  typedef date_time::null_dst_rules<ptime::date_type, time_duration> no_dst;
  //! Define US dst rule calculator for the posix_time system
  typedef date_time::us_dst_rules<ptime::date_type, time_duration> us_dst;


} } //namespace posix_time



/* Copyright (c) 2002
 * CrystalClear Software, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  CrystalClear Software makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#endif

