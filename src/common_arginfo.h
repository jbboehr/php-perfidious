/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 33f39df6ee60c71ce503341b4699bf69d9a443da */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Perfidious_UnsupportedMetricException___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Metric_unit, 0, 0, Perfidious\\MetricUnit, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Perfidious_Sampler___construct arginfo_class_Perfidious_UnsupportedMetricException___construct

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Sampler_open, 0, 1, Perfidious\\Sampler, 0)
	ZEND_ARG_TYPE_INFO(0, metrics, IS_ARRAY, 0)
	ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, scope, Perfidious\\Scope, 0, "Perfidious\\Scope::CurrentThread")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Perfidious_Sampler_metrics, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Sampler_read, 0, 0, Perfidious\\Sample, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Perfidious_Sampler_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Perfidious_Sample___construct arginfo_class_Perfidious_UnsupportedMetricException___construct

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Perfidious_Sample_value, 0, 1, IS_LONG, 0)
	ZEND_ARG_OBJ_INFO(0, metric, Perfidious\\Metric, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Sample_since, 0, 1, Perfidious\\SampleDelta, 0)
	ZEND_ARG_OBJ_INFO(0, earlier, Perfidious\\Sample, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Perfidious_SampleDelta___construct arginfo_class_Perfidious_UnsupportedMetricException___construct

#define arginfo_class_Perfidious_SampleDelta_value arginfo_class_Perfidious_Sample_value

