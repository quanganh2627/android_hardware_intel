/*! \file macros.h
 *  \brief helpful macros for Unipoint code.
 *  \date Jun 17, 2010
 *  \author praem@sensorstar.net
 */
#ifndef MACROS_H_
#define MACROS_H_

/*! \brief Provides unified error reporting on standard error.
 */
#define unipoint_error(...) \
	fprintf(stderr, "Unipoint error: ");\
	fprintf(stderr, __VA_ARGS__)

#define unipoint_max(a,b) \
	({typedef _ta = (a), _tb = (b);  \
	 _ta _a = (a); _tb _b = (b);     \
	 _a > _b ? _a : _b; })

#define unipoint_min(a,b) \
	({typedef _ta = (a), _tb = (b);  \
	 _ta _a = (a); _tb _b = (b);     \
	 _a < _b ? _a : _b; })

#define UNUSED(x) ((void)(x))

#define U16_MAX ((1 << 16) - 1)

#define LIKELY(x) __builtin_expect((x), 1)

#define UNLIKELY(x) __builtin_expect((x), 0)

#endif/*MACROS_H*/
