ENTRY(pattern_main)

#define att_media_memory_start  0x74000

#define CODE_LENGTH  0xa00
#define DATA_LENGTH  0x600

MEMORY
{
	pattern_ibank : ORIGIN = att_media_memory_start, LENGTH = CODE_LENGTH
	pattern_data  : ORIGIN = att_media_memory_start + CODE_LENGTH, LENGTH = DATA_LENGTH
	flash	      : ORIGIN = 0x00100000, LENGTH = 0x100000
}

SECTIONS
{
	/DISCARD/ :
	{
	   *(.comment)
	   *(.eh_frame)
	   *(.eh_frame_hdr)
	   *(.eh_frame_entry)
	   *(.MIPS.abiflags)
	   *(.MIPS.options)
	   *(.options)
	   *(.pdr)
	   *(.reginfo)
	}

	.text :
	{
		__core_c_code_start = .;
		KEEP(*(.entry))
		*(.text*)
		*(.rodata*)
		. = ALIGN(8);
		__core_c_code_end = .;

		__data_ram_start = .;
		*(.data*)
		. = ALIGN(8);
		__data_ram_end = .;
	} >pattern_ibank AT>flash

	.bss :
	{
		__bss_start = .;
		*(.bss*)
		*(COMMON*)
		*(.sbss)
		*(.common)
		*(common)
		. = ALIGN(4);
		__bss_end = .;
	} >pattern_data
}

