/* separate.c */
synt_tree *merge_synt_trees(grammar *g, synt_tree *left, synt_tree *right);
synt_tree *read_synt_tree(char *file);
synt_tree *read_synt_tree_fh(FILE *fh);
void write_synt_tree(char *file, synt_tree *t);
