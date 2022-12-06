# ----------------------------
# Makefile Options
# ----------------------------

NAME = MATHARC
DESCRIPTION = "A collection of fun games to play"
COMPRESSED = NO
ARCHIVED = YES

CFLAGS = -Wall -Wextra -Oz 
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)

.PHONY = CEmu cemu sprites

CEmu cemu: all
	$@ -s bin/$(NAME).8xp &

sprites:
	cd src/sprites/ && convimg
