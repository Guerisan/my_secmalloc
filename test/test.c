#include <criterion/criterion.h>
#include <criterion/redirect.h>
#define _GNU_SOURCE 
#include <sys/mman.h>
#include <stdio.h>
#include "my_secmalloc.h"
#include "my_secmalloc_private.h"

Test(mmap, simple) 
{
    printf("Ici on fait un test simple de mmap\n");
    char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    cr_expect(ptr != NULL);
    // On écrit des X à chaque octet de mémoire
    for (size_t i = 0; i < 4096; i+= 1)
      ptr[i] = 'X';
    // On vérifie qu'un index au pif contient bien un des X, puis qu'ils sont sensés être partout
    cr_expect(ptr[199] == 'X');
    // On libère, et on s'assure que munmap a bien renvoyé 0 et non un pointeur
    int res = munmap(ptr, 4096);
    cr_expect(res == 0);
}

Test(log, test_log, .init=cr_redirect_stderr) 
{
  my_log("Bonjour %d\n", 12);
  cr_assert_stderr_eq_str("Bonjour 12\n");
}

Test(canary, alloc)
{
  // On appelle malloc avec une taille de 12
  size_t szdata = 12;
  // Il faut réserver pour la szdata plus le canary
  size_t size = szdata + sizeof (size_t);
  // Allocation de la mémoire
  char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  // Nettoyage de la mémoire
  memset(ptr, 0, szdata);
  // Remplissage du canary
  for (size_t i = 0; i < sizeof (size_t); i++) 
    ptr[szdata + i] = 'X';
  cr_expect(ptr[szdata] == 'X');
  cr_expect(ptr[szdata -1] == 0);
  cr_expect(ptr[size] == 0);
 
}

Test(metainf, alloc)
{
  // Allocation du pool de metadata
  pool_meta_init(); 
  // Init 1 pool de datstruct dmem *dma
  pool_data_init(pool_meta);
  // Conf 1 descripteur
  int first = dmem_first_free(); // On récupère le premier descripteur 
  if (first == -1) {
    //TODO Et si pas de descripteur libre ? Remap ?
  }
  cr_expect(pool_meta[first].used == 1); // Après un init, le premier descripteur est pointé comme utilisé
  cr_expect(pool_meta[first].busy == 0);  // Mais libre
  cr_expect(pool_meta[first].data == pool_data); // Comme c'est le premier, data pointe sur pool_data
  dmem_init(first, 12); // On initialise ce premier descripteut à 12 octets
  cr_assert(pool_meta[first].sz = 12); // La taille du bloc récupéré devra à terme être à 12
  cr_assert(pool_meta[first].full = 12 + sizeof (size_t)); // L'estpace full est la taille allouée + le canary
  cr_assert(pool_meta[first].data = pool_data);
  cr_assert(pool_meta[first].busy = 1);
  cr_expect(pool_meta[first].data[pool_meta[first].sz] == 'X');
  cr_expect(pool_meta[first].data[pool_meta[first].sz - 1] == 0);
  cr_expect(pool_meta[first].data[pool_meta[first].full] == 0);
  size_t rest = dmem_first_free(); // Si on demande le bloc suivant, on tombe en principe sur le rest
  cr_assert(pool_meta[rest].data = pool_data + pool_meta[first].full); // Pointe sur la fin du bloc précédent
}


Test(my_simple_malloc, alloc)
{
  cr_expect(my_malloc(20) != NULL);
  my_log("Taille du premier descripteur : %d \n", pool_meta[0].sz);
  cr_expect(pool_meta[0].sz == 20);
  my_log("Taille avec canary : %d \n", pool_meta[0].full);
  cr_assert(pool_meta[0].full == (20 + sizeof(size_t)));
  my_log("Occupé : %d, Utilisé : %d \n", pool_meta[0].busy, pool_meta[0].used);
  cr_expect(pool_meta[0].busy == 1 && pool_meta[0].used == 1);
}

Test(my_simple_free, free)
{
  // On met des trucs dans la mémoire
  my_malloc(20);
  void *ptr = my_malloc(80);
  my_malloc(12);

  // Mais il nous faut son index pour récupérer le résultat
  long idx = -1;
  for (size_t i = 0; i < size_pool_metainf; i += 1) {
    if (pool_meta[i].data == ptr) {
      idx = i;
      break;
    }
  }

  cr_assert(pool_meta[idx].busy == 1);
  // Et on vérifie que my_free a bien remplis son rôle
  my_free(ptr);
  cr_assert(pool_meta[idx].busy == 0);
}

Test(meta_printer, print)
{
  //void *ptr1 = 
  my_malloc(20);
  void *ptr2 = my_malloc(80);
  //void *ptr3 = 
  my_malloc(12);
  // On essaie de visualiser l'état de notre mémoire pour s'y retrouver
  print_memory();

  my_free(ptr2);
  print_memory();
}

Test(my_canary_check, free)
{
  void *ptr = my_malloc(12);

  pool_meta[0].data[12 + 3] = 'Y';

  my_free(ptr);
  // On vérifie que le free n'a eu aucun effet sur la mémoire allouée :
  cr_assert(pool_meta[0].busy == 1);
  // Le script de test ne s'arrête plus avec cette assertion :
  //cr_assert_stderr_eq_str("Canary compromis à size 15\n");
}

Test(my_secmalloc, calloc_basic) {
    size_t nmemb = 5;
    size_t size = sizeof(int);
    int *ptr = (int *)my_calloc(nmemb, size);
    cr_assert_not_null(ptr, "my_calloc(nmemb, size) a retourné NULL.");

    for (size_t i = 0; i < nmemb; ++i) {
        cr_assert_eq(ptr[i], 0, "L'élément %zu n'est pas initialisé à zéro.", i);
    }
    my_free(ptr);
}

Test(my_secmalloc, realloc_basic) {
    int *ptr = my_malloc(5 * sizeof(int));
    cr_assert_not_null(ptr, "my_malloc(5 * sizeof(int)) a retourné NULL.");

    int *new_ptr = my_realloc(ptr, 10 * sizeof(int));
    cr_assert_not_null(new_ptr, "my_realloc(ptr, 10 * sizeof(int)) a retourné NULL.");

    my_free(new_ptr);
}

Test(my_secmalloc, realloc_to_smaller_size) {
    int *ptr = my_malloc(10 * sizeof(int));
    cr_assert_not_null(ptr, "my_malloc(10 * sizeof(int)) a retourné NULL.");

    int *new_ptr = my_realloc(ptr, 5 * sizeof(int));
    cr_assert_not_null(new_ptr, "my_realloc(ptr, 5 * sizeof(int)) a retourné NULL.");
    my_free(new_ptr);
}
