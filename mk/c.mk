# rule to generate .o and includable .d to properly handle .h files changes
%.o: %.c
	$(CC) $(CFLAGS) -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.o: %.cpp
	$(CC) $(CFLAGS) -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.o: %.cxx
	$(CC) $(CFLAGS) -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
