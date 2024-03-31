
src/pmu_ht.c: src/pmu_ht.h

src/pmu_ht.h: src/pmu_ht.gperf
	$(GPERF) --struct-type --readonly-tables --compare-strncmp --compare-lengths --global-table --output-file=src/pmu_ht.h src/pmu_ht.gperf
