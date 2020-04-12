/* $Id: trie.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
typedef struct trie_s *trie_ptr;

extern void trie_insert PROTO((trie_ptr *r,char *s,void *value));
extern void *trie_lookup PROTO((trie_ptr *r,char **sp));
