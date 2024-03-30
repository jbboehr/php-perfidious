--TEST--
list-pmu-events
--EXTENSIONS--
perf
--FILE--
<?php
var_dump(PerfExt\list_pmu_events(PerfExt\PmuEnum::PFM_PMU_AMD64_FAM19H_ZEN4));
--EXPECT--
array(77) {
  [0]=>
  array(2) {
    ["name"]=>
    string(37) "amd64_fam19h_zen4::RETIRED_X87_FP_OPS"
    ["is_present"]=>
    bool(false)
  }
  [1]=>
  array(2) {
    ["name"]=>
    string(40) "amd64_fam19h_zen4::RETIRED_SSE_AVX_FLOPS"
    ["is_present"]=>
    bool(false)
  }
  [2]=>
  array(2) {
    ["name"]=>
    string(42) "amd64_fam19h_zen4::RETIRED_SERIALIZING_OPS"
    ["is_present"]=>
    bool(false)
  }
  [3]=>
  array(2) {
    ["name"]=>
    string(42) "amd64_fam19h_zen4::RETIRED_FP_OPS_BY_WIDTH"
    ["is_present"]=>
    bool(false)
  }
  [4]=>
  array(2) {
    ["name"]=>
    string(41) "amd64_fam19h_zen4::RETIRED_FP_OPS_BY_TYPE"
    ["is_present"]=>
    bool(false)
  }
  [5]=>
  array(2) {
    ["name"]=>
    string(34) "amd64_fam19h_zen4::RETIRED_INT_OPS"
    ["is_present"]=>
    bool(false)
  }
  [6]=>
  array(2) {
    ["name"]=>
    string(40) "amd64_fam19h_zen4::PACKED_FP_OPS_RETIRED"
    ["is_present"]=>
    bool(false)
  }
  [7]=>
  array(2) {
    ["name"]=>
    string(41) "amd64_fam19h_zen4::PACKED_INT_OPS_RETIRED"
    ["is_present"]=>
    bool(false)
  }
  [8]=>
  array(2) {
    ["name"]=>
    string(37) "amd64_fam19h_zen4::FP_DISPATCH_FAULTS"
    ["is_present"]=>
    bool(false)
  }
  [9]=>
  array(2) {
    ["name"]=>
    string(31) "amd64_fam19h_zen4::BAD_STATUS_2"
    ["is_present"]=>
    bool(false)
  }
  [10]=>
  array(2) {
    ["name"]=>
    string(44) "amd64_fam19h_zen4::RETIRED_LOCK_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [11]=>
  array(2) {
    ["name"]=>
    string(47) "amd64_fam19h_zen4::RETIRED_CLFLUSH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [12]=>
  array(2) {
    ["name"]=>
    string(45) "amd64_fam19h_zen4::RETIRED_CPUID_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [13]=>
  array(2) {
    ["name"]=>
    string(30) "amd64_fam19h_zen4::LS_DISPATCH"
    ["is_present"]=>
    bool(false)
  }
  [14]=>
  array(2) {
    ["name"]=>
    string(31) "amd64_fam19h_zen4::SMI_RECEIVED"
    ["is_present"]=>
    bool(false)
  }
  [15]=>
  array(2) {
    ["name"]=>
    string(34) "amd64_fam19h_zen4::INTERRUPT_TAKEN"
    ["is_present"]=>
    bool(false)
  }
  [16]=>
  array(2) {
    ["name"]=>
    string(40) "amd64_fam19h_zen4::STORE_TO_LOAD_FORWARD"
    ["is_present"]=>
    bool(false)
  }
  [17]=>
  array(2) {
    ["name"]=>
    string(41) "amd64_fam19h_zen4::STORE_COMMIT_CANCELS_2"
    ["is_present"]=>
    bool(false)
  }
  [18]=>
  array(2) {
    ["name"]=>
    string(41) "amd64_fam19h_zen4::MAB_ALLOCATION_BY_TYPE"
    ["is_present"]=>
    bool(false)
  }
  [19]=>
  array(2) {
    ["name"]=>
    string(54) "amd64_fam19h_zen4::DEMAND_DATA_CACHE_FILLS_FROM_SYSTEM"
    ["is_present"]=>
    bool(false)
  }
  [20]=>
  array(2) {
    ["name"]=>
    string(51) "amd64_fam19h_zen4::ANY_DATA_CACHE_FILLS_FROM_SYSTEM"
    ["is_present"]=>
    bool(false)
  }
  [21]=>
  array(2) {
    ["name"]=>
    string(31) "amd64_fam19h_zen4::L1_DTLB_MISS"
    ["is_present"]=>
    bool(false)
  }
  [22]=>
  array(2) {
    ["name"]=>
    string(35) "amd64_fam19h_zen4::MISALIGNED_LOADS"
    ["is_present"]=>
    bool(false)
  }
  [23]=>
  array(2) {
    ["name"]=>
    string(51) "amd64_fam19h_zen4::PREFETCH_INSTRUCTIONS_DISPATCHED"
    ["is_present"]=>
    bool(false)
  }
  [24]=>
  array(2) {
    ["name"]=>
    string(48) "amd64_fam19h_zen4::INEFFECTIVE_SOFTWARE_PREFETCH"
    ["is_present"]=>
    bool(false)
  }
  [25]=>
  array(2) {
    ["name"]=>
    string(53) "amd64_fam19h_zen4::SOFTWARE_PREFETCH_DATA_CACHE_FILLS"
    ["is_present"]=>
    bool(false)
  }
  [26]=>
  array(2) {
    ["name"]=>
    string(53) "amd64_fam19h_zen4::HARDWARE_PREFETCH_DATA_CACHE_FILLS"
    ["is_present"]=>
    bool(false)
  }
  [27]=>
  array(2) {
    ["name"]=>
    string(34) "amd64_fam19h_zen4::ALLOC_MAB_COUNT"
    ["is_present"]=>
    bool(false)
  }
  [28]=>
  array(2) {
    ["name"]=>
    string(37) "amd64_fam19h_zen4::CYCLES_NOT_IN_HALT"
    ["is_present"]=>
    bool(false)
  }
  [29]=>
  array(2) {
    ["name"]=>
    string(30) "amd64_fam19h_zen4::TLB_FLUSHES"
    ["is_present"]=>
    bool(false)
  }
  [30]=>
  array(2) {
    ["name"]=>
    string(45) "amd64_fam19h_zen4::P0_FREQ_CYCLES_NOT_IN_HALT"
    ["is_present"]=>
    bool(false)
  }
  [31]=>
  array(2) {
    ["name"]=>
    string(52) "amd64_fam19h_zen4::INSTRUCTION_CACHE_REFILLS_FROM_L2"
    ["is_present"]=>
    bool(false)
  }
  [32]=>
  array(2) {
    ["name"]=>
    string(56) "amd64_fam19h_zen4::INSTRUCTION_CACHE_REFILLS_FROM_SYSTEM"
    ["is_present"]=>
    bool(false)
  }
  [33]=>
  array(2) {
    ["name"]=>
    string(43) "amd64_fam19h_zen4::L1_ITLB_MISS_L2_ITLB_HIT"
    ["is_present"]=>
    bool(false)
  }
  [34]=>
  array(2) {
    ["name"]=>
    string(44) "amd64_fam19h_zen4::L1_ITLB_MISS_L2_ITLB_MISS"
    ["is_present"]=>
    bool(false)
  }
  [35]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::L2_BTB_CORRECTION"
    ["is_present"]=>
    bool(false)
  }
  [36]=>
  array(2) {
    ["name"]=>
    string(47) "amd64_fam19h_zen4::DYNAMIC_INDIRECT_PREDICTIONS"
    ["is_present"]=>
    bool(false)
  }
  [37]=>
  array(2) {
    ["name"]=>
    string(47) "amd64_fam19h_zen4::DECODER_OVERRIDE_BRANCH_PRED"
    ["is_present"]=>
    bool(false)
  }
  [38]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::L1_ITLB_FETCH_HIT"
    ["is_present"]=>
    bool(false)
  }
  [39]=>
  array(2) {
    ["name"]=>
    string(26) "amd64_fam19h_zen4::RESYNCS"
    ["is_present"]=>
    bool(false)
  }
  [40]=>
  array(2) {
    ["name"]=>
    string(34) "amd64_fam19h_zen4::IC_TAG_HIT_MISS"
    ["is_present"]=>
    bool(false)
  }
  [41]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::OP_CACHE_HIT_MISS"
    ["is_present"]=>
    bool(false)
  }
  [42]=>
  array(2) {
    ["name"]=>
    string(34) "amd64_fam19h_zen4::OPS_QUEUE_EMPTY"
    ["is_present"]=>
    bool(false)
  }
  [43]=>
  array(2) {
    ["name"]=>
    string(53) "amd64_fam19h_zen4::OPS_SOURCE_DISPATCHED_FROM_DECODER"
    ["is_present"]=>
    bool(false)
  }
  [44]=>
  array(2) {
    ["name"]=>
    string(51) "amd64_fam19h_zen4::OPS_TYPE_DISPATCHED_FROM_DECODER"
    ["is_present"]=>
    bool(false)
  }
  [45]=>
  array(2) {
    ["name"]=>
    string(51) "amd64_fam19h_zen4::DISPATCH_RESOURCE_STALL_CYCLES_1"
    ["is_present"]=>
    bool(false)
  }
  [46]=>
  array(2) {
    ["name"]=>
    string(51) "amd64_fam19h_zen4::DISPATCH_RESOURCE_STALL_CYCLES_2"
    ["is_present"]=>
    bool(false)
  }
  [47]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::DISPATCH_STALLS_1"
    ["is_present"]=>
    bool(false)
  }
  [48]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::DISPATCH_STALLS_2"
    ["is_present"]=>
    bool(false)
  }
  [49]=>
  array(2) {
    ["name"]=>
    string(39) "amd64_fam19h_zen4::RETIRED_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [50]=>
  array(2) {
    ["name"]=>
    string(30) "amd64_fam19h_zen4::RETIRED_OPS"
    ["is_present"]=>
    bool(false)
  }
  [51]=>
  array(2) {
    ["name"]=>
    string(46) "amd64_fam19h_zen4::RETIRED_BRANCH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [52]=>
  array(2) {
    ["name"]=>
    string(59) "amd64_fam19h_zen4::RETIRED_BRANCH_INSTRUCTIONS_MISPREDICTED"
    ["is_present"]=>
    bool(false)
  }
  [53]=>
  array(2) {
    ["name"]=>
    string(52) "amd64_fam19h_zen4::RETIRED_TAKEN_BRANCH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [54]=>
  array(2) {
    ["name"]=>
    string(65) "amd64_fam19h_zen4::RETIRED_TAKEN_BRANCH_INSTRUCTIONS_MISPREDICTED"
    ["is_present"]=>
    bool(false)
  }
  [55]=>
  array(2) {
    ["name"]=>
    string(48) "amd64_fam19h_zen4::RETIRED_FAR_CONTROL_TRANSFERS"
    ["is_present"]=>
    bool(false)
  }
  [56]=>
  array(2) {
    ["name"]=>
    string(39) "amd64_fam19h_zen4::RETIRED_NEAR_RETURNS"
    ["is_present"]=>
    bool(false)
  }
  [57]=>
  array(2) {
    ["name"]=>
    string(52) "amd64_fam19h_zen4::RETIRED_NEAR_RETURNS_MISPREDICTED"
    ["is_present"]=>
    bool(false)
  }
  [58]=>
  array(2) {
    ["name"]=>
    string(68) "amd64_fam19h_zen4::RETIRED_INDIRECT_BRANCH_INSTRUCTIONS_MISPREDICTED"
    ["is_present"]=>
    bool(false)
  }
  [59]=>
  array(2) {
    ["name"]=>
    string(46) "amd64_fam19h_zen4::RETIRED_MMX_FP_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [60]=>
  array(2) {
    ["name"]=>
    string(55) "amd64_fam19h_zen4::RETIRED_INDIRECT_BRANCH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [61]=>
  array(2) {
    ["name"]=>
    string(58) "amd64_fam19h_zen4::RETIRED_CONDITIONAL_BRANCH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [62]=>
  array(2) {
    ["name"]=>
    string(40) "amd64_fam19h_zen4::DIV_CYCLES_BUSY_COUNT"
    ["is_present"]=>
    bool(false)
  }
  [63]=>
  array(2) {
    ["name"]=>
    string(31) "amd64_fam19h_zen4::DIV_OP_COUNT"
    ["is_present"]=>
    bool(false)
  }
  [64]=>
  array(2) {
    ["name"]=>
    string(35) "amd64_fam19h_zen4::CYCLES_NO_RETIRE"
    ["is_present"]=>
    bool(false)
  }
  [65]=>
  array(2) {
    ["name"]=>
    string(45) "amd64_fam19h_zen4::RETIRED_UCODE_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [66]=>
  array(2) {
    ["name"]=>
    string(36) "amd64_fam19h_zen4::RETIRED_UCODE_OPS"
    ["is_present"]=>
    bool(false)
  }
  [67]=>
  array(2) {
    ["name"]=>
    string(65) "amd64_fam19h_zen4::RETIRED_BRANCH_MISPREDICTED_DIRECTION_MISMATCH"
    ["is_present"]=>
    bool(false)
  }
  [68]=>
  array(2) {
    ["name"]=>
    string(82) "amd64_fam19h_zen4::RETIRED_UNCONDITIONAL_INDIRECT_BRANCH_INSTRUCTIONS_MISPREDICTED"
    ["is_present"]=>
    bool(false)
  }
  [69]=>
  array(2) {
    ["name"]=>
    string(60) "amd64_fam19h_zen4::RETIRED_UNCONDITIONAL_BRANCH_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [70]=>
  array(2) {
    ["name"]=>
    string(33) "amd64_fam19h_zen4::TAGGED_IBS_OPS"
    ["is_present"]=>
    bool(false)
  }
  [71]=>
  array(2) {
    ["name"]=>
    string(45) "amd64_fam19h_zen4::RETIRED_FUSED_INSTRUCTIONS"
    ["is_present"]=>
    bool(false)
  }
  [72]=>
  array(2) {
    ["name"]=>
    string(40) "amd64_fam19h_zen4::REQUESTS_TO_L2_GROUP1"
    ["is_present"]=>
    bool(false)
  }
  [73]=>
  array(2) {
    ["name"]=>
    string(61) "amd64_fam19h_zen4::CORE_TO_L2_CACHEABLE_REQUEST_ACCESS_STATUS"
    ["is_present"]=>
    bool(false)
  }
  [74]=>
  array(2) {
    ["name"]=>
    string(37) "amd64_fam19h_zen4::L2_PREFETCH_HIT_L2"
    ["is_present"]=>
    bool(false)
  }
  [75]=>
  array(2) {
    ["name"]=>
    string(37) "amd64_fam19h_zen4::L2_PREFETCH_HIT_L3"
    ["is_present"]=>
    bool(false)
  }
  [76]=>
  array(2) {
    ["name"]=>
    string(38) "amd64_fam19h_zen4::L2_PREFETCH_MISS_L3"
    ["is_present"]=>
    bool(false)
  }
}