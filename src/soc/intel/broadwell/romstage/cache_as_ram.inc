/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2000,2007 Ronald G. Minnich <rminnich@gmail.com>
 * Copyright (C) 2007-2008 coresystems GmbH
 * Copyright (C) 2014 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <cpu/x86/stack.h>
#include <cpu/x86/mtrr.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/post_code.h>
#include <cbmem.h>

/* The full cache-as-ram size includes the cache-as-ram portion from coreboot
 * and the space used by the reference code. These 2 values combined should
 * be a power of 2 because the MTRR setup assumes that. */
#define CACHE_AS_RAM_SIZE \
	(CONFIG_DCACHE_RAM_SIZE + CONFIG_DCACHE_RAM_MRC_VAR_SIZE)
#define CACHE_AS_RAM_BASE CONFIG_DCACHE_RAM_BASE
#define CACHE_AS_RAM_LIMIT (CONFIG_DCACHE_RAM_BASE + CONFIG_DCACHE_RAM_SIZE)
#define USBDEBUG_VAR_SIZE  36 /* sizeof(struct ehci_debug_info) */

/* Cache 4GB - MRC_SIZE_KB for MRC */
#define CACHE_MRC_BYTES   ((CONFIG_CACHE_MRC_SIZE_KB << 10) - 1)
#define CACHE_MRC_BASE    (0xFFFFFFFF - CACHE_MRC_BYTES)
#define CACHE_MRC_MASK    (~CACHE_MRC_BYTES)

#define CPU_MAXPHYSADDR CONFIG_CPU_ADDR_BITS
#define CPU_PHYSMASK_HI  (1 << (CPU_MAXPHYSADDR - 32) - 1)

#define NoEvictMod_MSR 0x2e0

	/* Save the BIST result. */
	movl	%eax, %ebp

cache_as_ram:
	post_code(0x20)

	/* Send INIT IPI to all excluding ourself. */
	movl	$0x000C4500, %eax
	movl	$0xFEE00300, %esi
	movl	%eax, (%esi)

	/* All CPUs need to be in Wait for SIPI state */
wait_for_sipi:
	movl	(%esi), %eax
	bt	$12, %eax
	jc	wait_for_sipi

	post_code(0x21)
	/* Zero out all fixed range and variable range MTRRs. */
	movl	$mtrr_table, %esi
	movl	$((mtrr_table_end - mtrr_table) / 2), %edi
	xorl	%eax, %eax
	xorl	%edx, %edx
clear_mtrrs:
	movw	(%esi), %bx
	movzx	%bx, %ecx
	wrmsr
	add	$2, %esi
	dec	%edi
	jnz	clear_mtrrs

	post_code(0x22)
	/* Configure the default memory type to uncacheable. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	andl	$(~0x00000cff), %eax
	wrmsr

	post_code(0x23)
	/* Set Cache-as-RAM base address. */
	movl	$(MTRRphysBase_MSR(0)), %ecx
	movl	$(CACHE_AS_RAM_BASE | MTRR_TYPE_WRBACK), %eax
	xorl	%edx, %edx
	wrmsr

	post_code(0x24)
	/* Set Cache-as-RAM mask. */
	movl	$(MTRRphysMask_MSR(0)), %ecx
	movl	$(~(CACHE_AS_RAM_SIZE - 1) | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx
	wrmsr

	post_code(0x25)

	/* Enable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	orl	$MTRRdefTypeEn, %eax
	wrmsr

	/* Enable cache (CR0.CD = 0, CR0.NW = 0). */
        movl	%cr0, %eax
	andl	$(~(CR0_CacheDisable | CR0_NoWriteThrough)), %eax
	invd
	movl	%eax, %cr0

	/* enable the 'no eviction' mode */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	orl     $1, %eax
	andl    $~2, %eax
	wrmsr

	/* Clear the cache memory region. This will also fill up the cache */
	movl	$CACHE_AS_RAM_BASE, %esi
	movl	%esi, %edi
	movl	$(CACHE_AS_RAM_SIZE / 4), %ecx
	xorl	%eax, %eax
	rep	stosl

	/* enable the 'no eviction run' state */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	orl     $3, %eax
	wrmsr

	post_code(0x26)
	/* Enable Cache-as-RAM mode by disabling cache. */
	movl	%cr0, %eax
	orl	$CR0_CacheDisable, %eax
	movl	%eax, %cr0

	/* Enable cache for our code in Flash because we do XIP here */
	movl	$MTRRphysBase_MSR(1), %ecx
	xorl	%edx, %edx
	/*
	 * IMPORTANT: The following calculation _must_ be done at runtime. See
	 * http://www.coreboot.org/pipermail/coreboot/2010-October/060855.html
	 */
	movl    $copy_and_run, %eax
	andl    $(~(CONFIG_XIP_ROM_SIZE - 1)), %eax
	orl	$MTRR_TYPE_WRPROT, %eax
	wrmsr

	movl	$MTRRphysMask_MSR(1), %ecx
	movl	$CPU_PHYSMASK_HI, %edx
	movl	$(~(CONFIG_XIP_ROM_SIZE - 1) | MTRRphysMaskValid), %eax
	wrmsr

	post_code(0x27)
#if CONFIG_CACHE_MRC_BIN
	/* Enable caching for ram init code to run faster */
	movl	$MTRRphysBase_MSR(2), %ecx
	movl	$(CACHE_MRC_BASE | MTRR_TYPE_WRPROT), %eax
	xorl	%edx, %edx
	wrmsr
	movl	$MTRRphysMask_MSR(2), %ecx
	movl	$(CACHE_MRC_MASK | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx
	wrmsr
#endif

	post_code(0x28)
	/* Enable cache. */
	movl	%cr0, %eax
	andl	$(~(CR0_CacheDisable | CR0_NoWriteThrough)), %eax
	movl	%eax, %cr0

	/* Setup the stack. */
	movl	$(CACHE_AS_RAM_LIMIT), %eax
#if CONFIG_USBDEBUG
	sub	$(USBDEBUG_VAR_SIZE), %eax
#endif
	movl	%eax, %esp

	/* Restore the BIST result. */
	movl	%ebp, %eax

	/* Build the call frame. */
	movl	%esp, %ebp
	pushl	$0 /* chipset_context */
	movd	%mm1, %ebx
	pushl	%ebx
	movd	%mm0, %ebx
	pushl	%ebx
	pushl	%eax

before_romstage:
	post_code(0x29)
	/* Call romstage.c main function. */
	call	romstage_main
	/* Save return value from romstage_main. It contains the stack to use
	 * after cache-as-ram is torn down. It also contains the information
	 * for setting up MTRRs. */
	movl	%eax, %ebx

	post_code(0x2f)

	/* Copy global variable space (for USBDEBUG) to memory */
#if CONFIG_USBDEBUG
	cld
	movl	$(CACHE_AS_RAM_LIMIT - USBDEBUG_VAR_SIZE), %esi
	movl	$(CONFIG_RAMTOP - USBDEBUG_VAR_SIZE), %edi
	movl	$USBDEBUG_VAR_SIZE, %ecx
	rep	movsb
#endif

	post_code(0x30)

	/* Disable cache. */
	movl	%cr0, %eax
	orl	$CR0_CacheDisable, %eax
	movl	%eax, %cr0

	post_code(0x31)

	/* Disable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	andl	$(~MTRRdefTypeEn), %eax
	wrmsr

	post_code(0x31)

	/* Disable the no eviction run state */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	andl    $~2, %eax
	wrmsr

	invd

	/* Disable the no eviction mode */
	rdmsr
	andl    $~1, %eax
	wrmsr

#if CONFIG_CACHE_MRC_BIN
	/* Clear MTRR that was used to cache MRC */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$MTRRphysBase_MSR(2), %ecx
	wrmsr
	movl	$MTRRphysMask_MSR(2), %ecx
	wrmsr
#endif

	post_code(0x33)

	/* Enable cache. */
	movl	%cr0, %eax
	andl	$~(CR0_CacheDisable | CR0_NoWriteThrough), %eax
	movl	%eax, %cr0

	post_code(0x36)

	/* Disable cache. */
	movl	%cr0, %eax
	orl	$CR0_CacheDisable, %eax
	movl	%eax, %cr0

	post_code(0x38)

	/* Setup stack as indicated by return value from ramstage_main(). */
	movl	%ebx, %esp

	/* Get number of MTRRs. */
	popl	%ebx
	movl	$MTRRphysBase_MSR(0), %ecx
1:
	testl	%ebx, %ebx
	jz	1f

	/* Low 32 bits of MTRR base. */
	popl	%eax
	/* Upper 32 bits of MTRR base. */
	popl	%edx
	/* Write MTRR base. */
	wrmsr
	inc	%ecx
	/* Low 32 bits of MTRR mask. */
	popl	%eax
	/* Upper 32 bits of MTRR mask. */
	popl	%edx
	/* Write MTRR mask. */
	wrmsr
	inc	%ecx

	dec	%ebx
	jmp	1b
1:
	post_code(0x39)

	/* And enable cache again after setting MTRRs. */
	movl	%cr0, %eax
	andl	$~(CR0_CacheDisable | CR0_NoWriteThrough), %eax
	movl	%eax, %cr0

	post_code(0x3a)

	/* Enable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	orl	$MTRRdefTypeEn, %eax
	wrmsr

	post_code(0x3b)

	/* Invalidate the cache again. */
	invd

	post_code(0x3c)

__main:
	post_code(POST_PREPARE_RAMSTAGE)
	cld			/* Clear direction flag. */
	pushl	$0		/* chipset_context */
	call	romstage_after_car

.Lhlt:
	post_code(POST_DEAD_CODE)
	hlt
	jmp	.Lhlt

mtrr_table:
	/* Fixed MTRRs */
	.word 0x250, 0x258, 0x259
	.word 0x268, 0x269, 0x26A
	.word 0x26B, 0x26C, 0x26D
	.word 0x26E, 0x26F
	/* Variable MTRRs */
	.word 0x200, 0x201, 0x202, 0x203
	.word 0x204, 0x205, 0x206, 0x207
	.word 0x208, 0x209, 0x20A, 0x20B
	.word 0x20C, 0x20D, 0x20E, 0x20F
	.word 0x210, 0x211, 0x212, 0x213
mtrr_table_end:

