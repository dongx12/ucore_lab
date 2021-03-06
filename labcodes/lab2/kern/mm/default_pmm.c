#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Ming's chinese book "Data Structure -- C programming language"
*/
// LAB2 EXERCISE 1: 2012011361
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list (双向链表) implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    list_init(&free_list); //free_list是用来记录空闲的内存块
    nr_free = 0; //nr_free是记录空闲内存块的页的个数
}

static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    //首先需要初始化该空闲块中的每个页
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        SetPageProperty(p); //设置p->flags
        p->property = 0;    //设置p->property
        set_page_ref(p, 0); //设置 p->ref
        list_add_before(&free_list, &(p->page_link));
    }
    //base是该空闲块中的第一个page，并且是free的，故property赋值为n
    base->property = n;
    //最后更新nf_free的值
    nr_free += n;
}


//在free_list中寻找第一个空闲块（要求块大小>=n），调整该空闲块的大小，并返回分配的内存块的地址
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) { //如果所有空闲块的大小都不能满足分配要求，则返回
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    list_entry_t *pageList = &free_list;

    //寻找第一个空闲块
    while ((le = list_next(le)) != &free_list) {
        struct Page *block = le2page(le, page_link);
        if (block->property >= n) {
        	int index=0;
        	//如果找到了合适的空闲块，说明该块的前n个页可以被分配出去，此时需要做一些设置
        	for (index=0; index<n; index++) {
        		pageList = list_next(le);
        		struct Page *singlePage = le2page(pageList, page_link);
        		SetPageReserved(singlePage);    //PG_reserved =1
        		ClearPageProperty(singlePage);  //PG_property =0
        		list_del(le);                   //unlink the pages from free_list
        		le = pageList;
        	}
        	//如果该空闲块过大，需要再重新计算大小
        	if (block->property > n) {
        		(le2page(le,page_link))->property = block->property - n;
        	}
        	SetPageReserved(block);
        	ClearPageProperty(block);
        	nr_free -= n;
            return block;
        }
    }
    //如果没有找到返回NULL
    return NULL;
}

/*
default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
*               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
*                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
*               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
*               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
*/

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(PageReserved(base));

    //遍历空闲块链表，找到合适的位置插入回收的地址块
    list_entry_t *site = &free_list;
    struct Page *page = NULL;
    //寻找第一个空闲块
    while ((site = list_next(site)) != &free_list) {
    	page = le2page(site, page_link);
        if (page > base) {
            break;
        }
    }
    //插入回收的空闲块：按页插入
    for (page=base; page<(base+n); page++) {
    	list_add_before(site, &(page->page_link));
    }

    //重置该块的字段
    base->flags = 0;
    base->property = n;
    set_page_ref(base, 0);
    ClearPageProperty(base);
    SetPageProperty(base);

    //尝试合并地址相连的空闲地址块
    //先查看后一个空闲块
    page = le2page(site, page_link); //得到后一个空闲块起始地址
    if ((base+n) == page) {
    	base->property += page->property;
    	page->property = 0;
    }
    //后查看前一个空闲块
    site = list_prev(&(base->page_link));
    page = le2page(site, page_link); //此时并不是前一个空闲块，而是前一个page
    if ((site!= &free_list) && page == (base-1)) {
    	while (site!= &free_list) {
    		if (page->property > 0) {
    			page->property += base->property;
    			base->property =0;
    			break;
    		}
    		site = list_prev(site);
    		page = le2page(site, page_link);
    	}
    }
    nr_free += n;
    return;
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

