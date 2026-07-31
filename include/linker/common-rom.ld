	SECTION_DATA_PROLOGUE(initlevel, (OPTIONAL),)
	{
		DEVICE_INIT_SECTIONS()
	} GROUP_LINK_IN(ROMABLE_REGION)

	/* verify we don't have rogue .init_<something> initlevel sections */
	SECTION_DATA_PROLOGUE(initlevel_error, (OPTIONAL), )
	{
		DEVICE_INIT_UNDEFINED_SECTION()
	}
	ASSERT(SIZEOF(initlevel_error) == 0, "Undefined initialization levels used.")

	SECTION_DATA_PROLOGUE(initshell, (OPTIONAL),)
	{
		SHELL_INIT_SECTIONS()
	} GROUP_LINK_IN(ROMABLE_REGION)


#ifdef CONFIG_CPLUSPLUS
	SECTION_PROLOGUE(_CTOR_SECTION_NAME,,)
	{
		/*
		 * The compiler fills the constructor pointers table below,
		 * hence symbol __CTOR_LIST__ must be aligned on 4 byte
		 * boundary.  To align with the C++ standard, the first elment
		 * of the array contains the number of actual constructors. The
		 * last element is NULL.
		 */
		. = ALIGN(4);
		__CTOR_LIST__ = .;
		LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
		KEEP(*(SORT_BY_NAME(".ctors*")))
		LONG(0)
		__CTOR_END__ = .;
	} GROUP_LINK_IN(ROMABLE_REGION)

	SECTION_PROLOGUE(init_array, (OPTIONAL),)
	{
		. = ALIGN(4);
		__init_array_start = .;
		KEEP(*(SORT_BY_NAME(".init_array*")))
		__init_array_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)
#endif

	SECTION_PROLOGUE (devconfig, (OPTIONAL),)
	{
		__devconfig_start = .;
		*(".devconfig.*")
		KEEP(*(SORT_BY_NAME(".devconfig*")))
		__devconfig_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)

	SECTION_PROLOGUE(net_l2, (OPTIONAL),)
	{
		__net_l2_start = .;
		*(".net_l2.init")
		KEEP(*(SORT_BY_NAME(".net_l2.init*")))
		__net_l2_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)

#if defined(CONFIG_BLUETOOTH)
	SECTION_DATA_PROLOGUE(_bt_channels_area,,SUBALIGN(4))
	{
		_bt_l2cap_fixed_chan_list_start = .;
		KEEP(*(SORT_BY_NAME("._bt_l2cap_fixed_chan.static.*")))
		_bt_l2cap_fixed_chan_list_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)
	
#if defined(CONFIG_BT_BREDR)
	SECTION_DATA_PROLOGUE(_bt_br_channels_area,,SUBALIGN(4))
	{
		_bt_l2cap_br_fixed_chan_list_start = .;
		KEEP(*(SORT_BY_NAME("._bt_l2cap_br_fixed_chan.static.*")))
		_bt_l2cap_br_fixed_chan_list_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)
#endif

	SECTION_DATA_PROLOGUE(_bt_gatt_over_br_user_info_area,,SUBALIGN(4))
	{
		_bt_gatt_over_br_user_info_list_start = .;
		KEEP(*(SORT_BY_NAME("._bt_gatt_over_br_user_info.static.*")))
		_bt_gatt_over_br_user_info_list_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)
	
	SECTION_DATA_PROLOGUE(_bt_services_area,,SUBALIGN(4))
	{
		_bt_gatt_service_static_list_start = .;
		KEEP(*(SORT_BY_NAME("._bt_gatt_service_static.static.*")))
		_bt_gatt_service_static_list_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)
#endif