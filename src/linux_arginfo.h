/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: ccfde18b35f0fbd3a346e91a93de1204516d8043 */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Perfidious_get_pmu_info, 0, 1, Perfidious\\PmuInfo, 0)
	ZEND_ARG_TYPE_INFO(0, pmu, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Perfidious_get_pmu_event_info, 0, 2, Perfidious\\PmuEventInfo, 0)
	ZEND_ARG_TYPE_INFO(0, pmu, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, idx, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Perfidious_list_pmus, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Perfidious_list_pmu_events, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, pmu, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Perfidious_open, 0, 1, Perfidious\\Handle, 0)
	ZEND_ARG_TYPE_INFO(0, event_names, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, pid, IS_LONG, 0, "0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, cpu, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Perfidious_request_handle, 0, 0, Perfidious\\Handle, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Perfidious_Handle_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Handle_enable, 0, 0, Perfidious\\Handle, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Perfidious_Handle_disable arginfo_class_Perfidious_Handle_enable

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Perfidious_Handle_rawStream, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, idx, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Perfidious_Handle_read, 0, 0, Perfidious\\ReadResult, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Perfidious_Handle_readArray arginfo_Perfidious_list_pmus

#define arginfo_class_Perfidious_Handle_reset arginfo_class_Perfidious_Handle_enable

