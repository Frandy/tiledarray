#ifndef TILEDARRAY_UNIT_TEST_CONFIG_H__INCLUDED
#define TILEDARRAY_UNIT_TEST_CONFIG_H__INCLUDED

#ifndef BOOST_TEST_DYN_LINK

/* Defines the boost unit test framework linkage. */
/* #undef BOOST_TEST_DYN_LINK 1 */

#endif // BOOST_TEST_DYN_LINK

#if defined(BOOST_TEST_MAIN) && !defined(BOOST_TEST_DYN_LINK)
#include <boost/test/included/unit_test.hpp>
#else
#include <boost/test/unit_test.hpp>
#endif // BOOST_TEST_MAIN

#include <boost/test/output_test_stream.hpp>
#include "global_fixture.h"
#include "iteration_test.h"

#endif // TILEDARRAY_UNIT_TEST_CONFIG_H__INCLUDED
