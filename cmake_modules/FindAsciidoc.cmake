# - Find Asciidoc
# this module looks for asciidoc
#
# ASCIIDOC_EXECUTABLE - the full path to asciidoc
# ASCIIDOC_A2X_EXECUTABLE - the full path to asciidoc's a2x
# ASCIIDOC_FOUND - If false, don't attempt to use asciidoc.
#
# Taken from:
# http://lissyx.dyndns.org/redmine/projects/qpdfpresenterconsole/repository/revisions/master/raw/cmake/FindAsciidoc.cmake

FIND_PROGRAM(ASCIIDOC_EXECUTABLE asciidoc)
FIND_PROGRAM(ASCIIDOC_A2X_EXECUTABLE a2x)

MARK_AS_ADVANCED(
ASCIIDOC_EXECUTABLE
ASCIIDOC_A2X_EXECUTABLE
)

IF ((NOT ASCIIDOC_EXECUTABLE) OR (NOT ASCIIDOC_A2X_EXECUTABLE))
SET(ASCIIDOC_FOUND "NO")
ELSE ((NOT ASCIIDOC_EXECUTABLE) OR (NOT ASCIIDOC_A2X_EXECUTABLE))
SET(ASCIIDOC_FOUND "YES")
ENDIF ((NOT ASCIIDOC_EXECUTABLE) OR (NOT ASCIIDOC_A2X_EXECUTABLE))

IF (NOT ASCIIDOC_FOUND AND Asciidoc_FIND_REQUIRED)
MESSAGE(FATAL_ERROR "Could not find asciidoc")
ENDIF (NOT ASCIIDOC_FOUND AND Asciidoc_FIND_REQUIRED)
