// cette définition permet d'accéder à mremap lorsqu'on inclue sys/mman.h
#define _GNU_SOURCE
#include <sys/mman.h>
//#define _ISOC99_SOURCE// Me sort une erreur au make test, car déjç déinifi à /usr/include/features.h:205:
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <dlfcn.h>

#include "my_secmalloc.h"
#include "my_secmalloc_private.h"


/*static*/ void *pool_data = 0;
/*static*/ struct dmem *pool_meta = 0;
/*static */size_t size_pool_data = 1024*1024;
/*static */size_t size_pool_metainf = 1024*1024*100;
size_t pool_meta_count = 0;

void print_memory ()
{
  size_t count = pool_meta_count;
  my_log("Meta : | ");
  for (size_t i = 0; i < count; i += 1) 
    my_log("%d - %d+%d - %d/%d | ", i, pool_meta[i].sz, sizeof(size_t), pool_meta[i].busy, pool_meta[i].used);

  my_log("\nData : | ");
  for (size_t i = 0; i < count; i += 1)
    my_log("%d - %d | ", i, sizeof(pool_data[i]) );
  my_log("\n");
}

size_t get_pool_metainf_size ()
{
  return size_pool_metainf;
}

void pool_meta_init()
{
  if(!pool_meta)
     pool_meta = mmap(NULL, get_pool_metainf_size(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  pool_meta_count += 1;
  memset(pool_meta, 0, sizeof (*pool_meta));
}

long dmem_first_free()
{
  for (unsigned int i = 0; i < (size_pool_metainf / sizeof (struct dmem)); i++ ) {
    if (pool_meta[i].busy == 0 && pool_meta[i].used == 1)  
      return i;
  }
  return -1;
}

long dmem_first_notused()
{
  for (unsigned int i = 0; i < (size_pool_metainf / sizeof (struct dmem)); i++) {
    if (pool_meta[i].used == 0)  
      return i;
  }

  return -1;
}

void pool_data_init(struct dmem *dm)
{
  if (!pool_data) 
     pool_data = mmap((void*) dm + size_pool_metainf, size_pool_data, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  // Au moment de l'allocation, on fait en sorte que le premier descriptor référence l'intégralité de la mémoire tel qu'elle existe à ce moment là dans le pool de data
  dm[0].data = pool_data;
  dm[0].full = size_pool_data;
  dm[0].busy = 0; // Le bloc est libre
  dm[0].used = 1; // Le descriptor est utilisé
}

// Prend un descripteur à l'index idx donné
void    dmem_init(size_t idx, size_t sz )
{
  struct dmem *dm = &pool_meta[idx];
  if(dm->used)
  {
    // Trouver un descriptor not used
    size_t rest = dmem_first_notused();
    // Et faire le découpage
    pool_meta[rest].used = 1;
    pool_meta[rest].busy = 0;
    pool_meta[rest].full = dm->full - (sz +  sizeof (size_t)); // Le bloc est full - la taille initialisée + le canary
    pool_meta[rest].data = dm->data + (sz +  sizeof (size_t));
  }
  else
  {
    //TODO quid si le descriptor demandé n'est pas utilisé ? (donc pas de connexion entre le bloc et la mémoire)
  }
  dm->sz = sz;
  dm->full = sz + sizeof (size_t);
  dm->busy = 1;
  // Nettoie la mémoire
  memset(dm->data, 0, dm->sz);
  // Remplis le canary
  for (size_t i = 0; i < sizeof (size_t); i++) {
    dm->data[dm->sz + i] = 'X';
  }
}


void    my_log(const char *fmt,  ...){
  va_list ap;

  va_start(ap, fmt);
  size_t sz = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  char *buf = alloca(sz + 2);

  va_start(ap, fmt);
  vsnprintf(buf, sz + 1, fmt, ap);
  va_end(ap);

  write(2, buf, sz);
}

void    *my_malloc(size_t size)
{
      // Vérification de la taille demandée
    if (size == 0) 
        return NULL;

    if(size > size_pool_data)
    {
      //TODO
      my_log("Plus assez de mémoire disponible (remap ?)\n");
      return NULL;
    }

    // Initialisation des pools de métadonnées et de données si nécessaire
    if (!pool_meta) {
        pool_meta_init();
    }
    if (!pool_data) {
        pool_data_init(pool_meta);
    }

    // Trouver un bloc de mémoire libre
    long idx = dmem_first_free();
    if (idx == -1) {
        // Aucun bloc libre trouvé, retourner NULL
        my_log("Aucun bloc libre trouvé\n");
        return NULL;
    }

    // Initialisation du descripteur avec la taille demandée
    dmem_init(idx, size);

    // On actualise le comptage de descripteurs
    pool_meta_count += 1;

    // Retourner le pointeur vers la zone mémoire allouée
    my_log("Malloc de %d\n", size);
    return pool_meta[idx].data;
}

void    my_free(void *ptr)
{
  // Vérifie que le pointeur donné en paramètre correspond bien à un descripteur dans le pool de meta-information
  if (ptr == NULL)
    return;
  
  // Recherche du bloc de métadonnées correspondant au pointeur
  long idx = -1;
  for (size_t i = 0; i < size_pool_metainf; i += 1) {
    if (pool_meta[i].data == ptr) {
      idx = i;
      break;
    }
  }

  // Si le bloc de métadonnées n'est pas trouvé, on return
  if (idx == -1) {
    return;
  }
  
  // Check de l'intégrité du canary
  for (size_t i = 0; i <= (pool_meta[idx].full - pool_meta[idx].sz); i++) {
    if (pool_meta[idx].data[pool_meta[idx].sz + i] != 'X')
    {
      my_log("Canary compromis à size + %d\n", i);
      return;
    }
  } 

  // Mettre à jour le bloc de métadonnées pour indiquer qu'il est libre

  my_log("free de %p\n", ptr);
  pool_meta[idx].busy = 0;

  
  // Gestion de la défragmentation
}

void    *my_calloc(size_t nmemb, size_t size)
{
  // Vérification de la taille demandée
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    // Calcul de la taille totale à allouer
    size_t total_size = nmemb * size;

    // Gestion des cas de débordement de taille
    if (total_size / nmemb != size) {
        return NULL;
    }

    // Allouer la mémoire avec my_malloc
    my_log("calloc");
    void *ptr = my_malloc(total_size);

    // Vérifier si l'allocation a réussi
    if (ptr == NULL) {
        return NULL;
    }

    // Initialiser la mémoire allouée à zéro avec memset
    memset(ptr, 0, total_size);

    // Retourner le pointeur vers la zone mémoire allouée et initialisée
    return ptr;
}

void    *my_realloc(void *ptr, size_t size)
{
   // Si le pointeur est NULL,  my_realloc() doit se comporter comme my_malloc()
    if (ptr == NULL) {
        return my_malloc(size);
    }

    // Si la taille est zéro, alors my_realloc() doit se comporter comme my_free()
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // Trouver le bloc de métadonnées correspondant au pointeur
    long idx = -1;
    for (size_t i = 0; i < size_pool_metainf; i += 1) {
        if (pool_meta[i].data == ptr) {
            idx = i;
            break;
        }
    }

    // Retourner null si non trouvé
    if (idx == -1) {
        return NULL;
    }

    // Si la nouvelle taille est égale à la taille actuelle, retourner le pointeur inchangé
    if (size == pool_meta[idx].sz) {
        return ptr;
    }
    

    // Allocation un nouvel espace mémoire de la taille demandée
    //void *new_ptr = my_malloc(size);
    //if (new_ptr == NULL) {
    //    return NULL;
    //}

    // Copie des données de l'ancien espace mémoire vers le nouveau
    //size_t min_size = size < pool_meta[idx].sz ? size : pool_meta[idx].sz;
    //memcpy(new_ptr, ptr, min_size);

    // Libérer l'ancien espace mémoire
    //my_free(ptr);


    // Version avem mremap:
    // Redimensionner le bloc de mémoire avec mremap
    void *new_ptr = mremap(ptr, pool_meta[idx].sz, size, MREMAP_MAYMOVE);
    if (new_ptr == MAP_FAILED) {
        // Si mremap échoue, retourner NULL
        return NULL;
    }

    // Mettre à jour les métadonnées pour refléter la nouvelle taille
    pool_meta[idx].sz = size;
    pool_meta[idx].data = new_ptr;
    my_log("reallocated");
    // Retourner le pointeur vers le nouvel espace mémoire
    return new_ptr;
}

#ifdef DYNAMIC
/*
 * Lorsque la bibliothèque sera compilé en .so les symboles malloc/free/calloc/realloc seront visible
 * */

void    *malloc(size_t size)
{
    return my_malloc(size);
}
void    free(void *ptr)
{
    my_free(ptr);
}
void    *calloc(size_t nmemb, size_t size)
{
    return my_calloc(nmemb, size);
}

void    *realloc(void *ptr, size_t size)
{
    return my_realloc(ptr, size);
}

#endif
