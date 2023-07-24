.section .rodata.vram_data

.align 2
.global vram_data
vram_data:
	.incbin "../output/vram0.tar"
.global vram_data_end
vram_data_end:
