dnl @synopsis VL_LIB_READLINE([ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl
dnl Searches for a readline compatible library. If found, defines
dnl `HAVE_LIBREADLINE'. If the found library has the `add_history'
dnl function, sets also `HAVE_READLINE_HISTORY'. Also checks for the
dnl locations of the necessary include files and sets `HAVE_READLINE_H'
dnl or `HAVE_READLINE_READLINE_H' and `HAVE_READLINE_HISTORY_H' or
dnl 'HAVE_HISTORY_H' if the corresponding include files exists.
dnl
dnl The libraries that may be readline compatible are `libedit',
dnl `libeditline' and `libreadline'. Sometimes we need to link a
dnl termcap library for readline to work, this macro tests these cases
dnl too by trying to link with `libtermcap', `libcurses' or
dnl `libncurses' before giving up.
dnl
dnl Here is an example of how to use the information provided by this
dnl macro to perform the necessary includes or declarations in a C
dnl file:
dnl
dnl   #ifdef HAVE_LIBREADLINE
dnl   #  if defined(HAVE_READLINE_READLINE_H)
dnl   #    include <readline/readline.h>
dnl   #  elif defined(HAVE_READLINE_H)
dnl   #    include <readline.h>
dnl   #  else /* !defined(HAVE_READLINE_H) */
dnl   extern char *readline ();
dnl   #  endif /* !defined(HAVE_READLINE_H) */
dnl   char *cmdline = NULL;
dnl   #else /* !defined(HAVE_READLINE_READLINE_H) */
dnl     /* no readline */
dnl   #endif /* HAVE_LIBREADLINE */
dnl
dnl   #ifdef HAVE_READLINE_HISTORY
dnl   #  if defined(HAVE_READLINE_HISTORY_H)
dnl   #    include <readline/history.h>
dnl   #  elif defined(HAVE_HISTORY_H)
dnl   #    include <history.h>
dnl   #  else /* !defined(HAVE_HISTORY_H) */
dnl   extern void add_history ();
dnl   extern int write_history ();
dnl   extern int read_history ();
dnl   #  endif /* defined(HAVE_READLINE_HISTORY_H) */
dnl     /* no history */
dnl   #endif /* HAVE_READLINE_HISTORY */
dnl
dnl Modifications to add --enable-gnu-readline to work around licensing
dnl problems between the traditional BSD licence and the GPL.
dnl Martin Ebourne, 2005/7/11
dnl Rewrite to match headers with libraries and be more selective.
dnl Martin Ebourne, 2006/1/4
dnl
dnl @category InstalledPackages
dnl @author Ville Laurikari <vl@iki.fi>
dnl @version 2002-04-04
dnl @license AllPermissive

AC_DEFUN([VL_LIB_READLINE], [
  AC_ARG_ENABLE(
    [gnu-readline],
    AC_HELP_STRING([--enable-gnu-readline],
                   [Use GNU readline if present (may violate GNU licence)])
  )
  vl_cv_lib_readline_compat_found=no
  if test "x$enable_gnu_readline" = "xyes"; then
    VL_LIB_READLINE_CHECK([readline],
                          [readline],
                          [readline/readline.h readline.h],
                          [readline/history.h history.h])
  fi
  if test "x$vl_cv_lib_readline_compat_found" = "xno"; then
    VL_LIB_READLINE_CHECK([editline],
                          [edit editline],
	  		  [editline/readline.h],
			  [editline/readline.h])
  fi
  if test "x$vl_cv_lib_readline_compat_found" = "xyes"; then
    m4_ifvaln([$1],[$1],[:])dnl
    m4_ifvaln([$2],[else $2])dnl
  fi
])

dnl BOX_CHECK_VAR(name, where, headers)
AC_DEFUN([BOX_CHECK_VAR], [
  AC_CACHE_CHECK([for $1 $2], [vl_cv_var_$1], 
    [AC_TRY_LINK([$3], [(void) $1], [vl_cv_var_$1=yes], [vl_cv_var_$1=no])
    ])
  if test "${vl_cv_var_$1}" = "yes"; then
    AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_$1), 1, [Define if you have $1 $2])
  fi
  ])

dnl VL_LIB_READLINE_CHECK(name, libraries, headers, history headers)
AC_DEFUN([VL_LIB_READLINE_CHECK], [
  ORIG_LIBS="$LIBS"
  AC_CACHE_CHECK([for $1 library],
                 [vl_cv_lib_$1], [
    vl_cv_lib_$1=""
    for readline_lib in $2; do
      for termcap_lib in "" termcap curses ncurses pdcurses; do
        if test -z "$termcap_lib"; then
          TRY_LIB="-l$readline_lib"
        else
          TRY_LIB="-l$readline_lib -l$termcap_lib"
        fi
        LIBS="$ORIG_LIBS $TRY_LIB"
        AC_TRY_LINK_FUNC([readline], [vl_cv_lib_$1="$TRY_LIB"])
        if test -n "$vl_cv_lib_$1"; then
          break
        fi
      done
      if test -n "$vl_cv_lib_$1"; then
        break
      fi
    done
    if test -z "$vl_cv_lib_$1"; then
      vl_cv_lib_$1=no
      LIBS="$ORIG_LIBS"
    fi
  ])

  vl_cv_lib_includes=""

  vl_cv_lib_readline_compat_found=no
  if test "x$vl_cv_lib_$1" != "xno"; then
    AC_CHECK_HEADERS([$3], [
      vl_cv_lib_readline_compat_found=yes
      vl_cv_lib_includes="$vl_cv_lib_headers #include <$ac_header>"
    ])
  fi

  AC_TRY_LINK([$vl_cv_lib_includes], [(void) readline;],
    [vl_compiles=yes], [vl_compiles=no])
  if test "x$vl_compiles" = "xno"; then
    AC_TRY_LINK([#include <stdio.h>
      $vl_cv_lib_includes], [(void) readline;],
      [vl_compiles_with_stdio=yes], [vl_compiles_with_stdio=no])
    if test "x$vl_compiles_with_stdio" = "xyes"; then
      vl_cv_lib_includes="#include <stdio.h>
$vl_cv_lib_includes"
    fi
  fi

  if test "x$vl_cv_lib_readline_compat_found" = "xyes"; then
    BOX_CHECK_VAR([rl_completion_matches], [in readline headers],
      [$vl_cv_lib_includes])

    BOX_CHECK_VAR([completion_matches], [in readline headers],
      [$vl_cv_lib_includes])

    AC_DEFINE([HAVE_LIBREADLINE], 1,
              [Define if you have a readline compatible library])

    AC_CACHE_CHECK([whether $1 supports history],
                   [vl_cv_lib_$1_history], [
      vl_cv_lib_$1_history=no
      AC_TRY_LINK_FUNC([add_history], [vl_cv_lib_$1_history=yes])
    ])
    if test "x$vl_cv_lib_$1_history" = "xyes"; then
      vl_cv_lib_$1_history=no
      AC_CHECK_HEADERS(
        [$4],
	[AC_DEFINE([HAVE_READLINE_HISTORY], [1],
                   [Define if your readline library has add_history])])
    fi
  else
    LIBS="$ORIG_LIBS"
  fi
])dnl
