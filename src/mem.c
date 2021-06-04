
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct
{
	uint32_t proc; // ID of process currently uses this page
	int index;		 // Index of the page in the list of pages allocated
			// to the process.
	int next; // The next page in the list. -1 if it is the last
						// page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void)
{
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr)
{
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr)
{
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr)
{
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t *get_page_table(
		addr_t index, // Segment level index
		struct seg_table_t *seg_table)
{ // first level table

	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++)
	{
		if (seg_table->table[i].v_index == index)
			return seg_table->table[i].pages;
	}
	return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr,	 // Given virtual address
		addr_t *physical_addr, // Physical address to be returned
		struct pcb_t *proc)
{ // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);

	/* Search in the first level */
	struct page_table_t *page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL)
	{
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++)
	{
		if (page_table->table[i].v_index == second_lv)
		{
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			*physical_addr = (page_table->table[i].p_index << OFFSET_LEN) + offset;
			return 1;
		}
	}
	return 0;
}

addr_t alloc_mem(uint32_t size, struct pcb_t *proc)
{
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE == 0) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0;																																			// We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	int temp = 0;
	int i;
	for (i = 0; i < NUM_PAGES; i++)
		if (_mem_stat[i].proc == 0)
			temp++;
	if (temp >= num_pages)
		mem_avail = 1;
	if (proc->bp + num_pages * PAGE_SIZE > (1 << ADDRESS_SIZE))
		mem_avail = 0;

	if (mem_avail)
	{
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		// Update [proc], [index], and [next] field
		uint32_t temp_num_pages = num_pages;
		uint32_t index = 0;
		uint32_t prev_page = -1;
		uint32_t first_page = -1;
		while (temp_num_pages > 0)
		{
			if (_mem_stat[index].proc == 0)
			{
				if (prev_page != -1)
					_mem_stat[prev_page].next = index;
				else
					first_page = index;
				_mem_stat[index].proc = proc->pid;
				_mem_stat[index].index = num_pages - temp_num_pages;
				temp_num_pages -= 1;
				prev_page = index;
			}
			index += 1;
		}
		_mem_stat[prev_page].next = -1;

		// Add entries to segment table page tables of [proc]
		temp_num_pages = num_pages;
		addr_t prev_bp = ret_mem;
		int seg_size, page_size;
		while (temp_num_pages != 0)
		{
			if (proc->seg_table->size == 0)
				proc->seg_table->size += 1;
			seg_size = proc->seg_table->size;
			if (proc->seg_table->table[seg_size - 1].pages == NULL)
			{
				proc->seg_table->table[seg_size - 1].pages = malloc(sizeof(struct page_table_t));
				proc->seg_table->table[seg_size - 1].pages->size = 0;
			}

			proc->seg_table->table[seg_size - 1].pages->size += 1;
			page_size = proc->seg_table->table[seg_size - 1].pages->size;

			if (page_size <= (1 << PAGE_LEN))
			{
				proc->seg_table->table[seg_size - 1].v_index = seg_size - 1;
				proc->seg_table->table[seg_size - 1].pages->table[page_size - 1].v_index = get_second_lv(prev_bp);
				proc->seg_table->table[seg_size - 1].pages->table[page_size - 1].p_index = first_page;
				first_page = _mem_stat[first_page].next;
				temp_num_pages -= 1;
				prev_bp = prev_bp + PAGE_SIZE;
			}
			else
			{
				proc->seg_table->size += 1;
				proc->seg_table->table[seg_size - 1].pages->size -= 1;
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t *proc)
{
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */
	pthread_mutex_lock(&mem_lock);
	addr_t physical_addr;
	int num_pages = 0;
	int i;
	if (translate(address, &physical_addr, proc))
	{
		addr_t physical_page = physical_addr >> OFFSET_LEN;
		while (physical_page != -1)
		{
			_mem_stat[physical_page].proc = 0;
			physical_page = _mem_stat[physical_page].next;
			num_pages += 1;
		}
		addr_t virtual_addr = address;

		while (num_pages != 0)
		{
			addr_t first_lv = get_first_lv(virtual_addr);
			addr_t second_lv = get_second_lv(virtual_addr);
			struct page_table_t *page_table = NULL;
			page_table = get_page_table(first_lv, proc->seg_table);
			if (page_table != NULL)
			{
				for (i = 0; i < (1 << PAGE_LEN); i++)
					if (second_lv == page_table->table[i].v_index)
					{
						page_table->table[i].v_index -= 1;
						page_table->table[i].p_index -= 1;
						virtual_addr = virtual_addr + PAGE_SIZE;
						num_pages -= 1;
						break;
					}
			}
			else
			{
				break;
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t *proc, BYTE *data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		*data = _ram[physical_addr];
		return 0;
	}
	else
	{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t *proc, BYTE data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		_ram[physical_addr] = data;
		return 0;
	}
	else
	{
		return 1;
	}
}

void dump(void)
{
	int i;
	for (i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc != 0)
		{
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
						 i << OFFSET_LEN,
						 ((i + 1) << OFFSET_LEN) - 1,
						 _mem_stat[i].proc,
						 _mem_stat[i].index,
						 _mem_stat[i].next);
			int j;
			for (j = i << OFFSET_LEN;
					 j < ((i + 1) << OFFSET_LEN) - 1;
					 j++)
			{

				if (_ram[j] != 0)
				{
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
			}
		}
	}
}
