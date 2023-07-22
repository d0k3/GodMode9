.section .rodata.vram_data

.align 2
.global vram_data
vram_data:
	.incbin "../output/vram0.tar"
vram_data_end:

.align 2
.global vram_data_size
vram_data_size:
	.word vram_data_end - vram_data
