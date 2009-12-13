AM_CFLAGS = -Wall -Wstrict-prototypes -Wmissing-prototypes

bin_PROGRAMS = fm fmscan

fm_SOURCES = fm.c fmlib.c fmlib.h
fm_LDADD = -lm

fmscan_SOURCES = fmscan.c fmlib.c fmlib.h
fmscan_LDADD = -lm

#### The rest of this file is just for the testsuite.  It is just
#### boilerplate from the Autoconf manual describing how to use
#### Autotest.

# The `:;' works around a Bash 3.2 bug when the output is not writeable.
$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	:;{ \
	  echo '# Signature of the current package.' && \
	  echo 'm4_define([AT_PACKAGE_NAME],' && \
	  echo '  [@PACKAGE_NAME@])' && \
	  echo 'm4_define([AT_PACKAGE_TARNAME],' && \
	  echo '  [@PACKAGE_TARNAME@])' && \
	  echo 'm4_define([AT_PACKAGE_VERSION],' && \
	  echo '  [@PACKAGE_VERSION@])' && \
	  echo 'm4_define([AT_PACKAGE_STRING],' && \
	  echo '  [@PACKAGE_STRING@])' && \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT],' && \
	  echo '  [@PACKAGE_BUGREPORT@])'; \
	  echo 'm4_define([AT_PACKAGE_URL],' && \
	  echo '  [@PACKAGE_URL@])'; \
	} >'$(srcdir)/package.m4'

EXTRA_DIST = testsuite.at $(srcdir)/package.m4 $(TESTSUITE) atlocal.in
DISTCLEANFILES = atconfig
TESTSUITE = $(srcdir)/testsuite
TESTSUITE_AT = \
	$(srcdir)/testsuite.at \
	$(srcdir)/test-fm.at \
	$(srcdir)/test-fmscan.at \
	$(srcdir)/package.m4

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

installcheck-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	  $(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || \
	 $(SHELL) '$(TESTSUITE)' --clean

AUTOM4TE = $(SHELL) $(srcdir)/build-aux/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@
