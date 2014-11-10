CXXFLAGS	:= -I../../include $(CXXFLAGS)
export

SOURCE		= $(wildcard *.c)
OBJECTS		= $(SOURCE:.c=_$(common_suffix).o)

.PHONY: all
all: $(OBJECTS)

%_$(common_suffix).o: %.c
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(SOURCE:.c=.d)

.PHONY: clean
clean:
	rm -f *.o *.d
