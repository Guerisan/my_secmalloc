#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

/*
 * Ici vous pourrez faire toutes les déclarations de variables/fonctions pour votre usage interne
 * */

void    my_log(const char *fmt, ...);

struct  dmem
{
  char    *data;
  size_t  sz;
  size_t  full; // sz avec canary
  char    busy;
  char    used;
};

void    dmem_init(size_t idx, size_t sz );
size_t get_pool_metainf_size ();

void print_memory();
void pool_data_init(struct dmem *);
void pool_meta_init();
long dmem_first_free();
long dmem_first_notused();

extern size_t pool_meta_count;

#if TEST
extern void *pool_data;
extern struct dmem *pool_meta;
extern size_t size_pool_data;
extern size_t size_pool_metainf;
#endif

#endif
