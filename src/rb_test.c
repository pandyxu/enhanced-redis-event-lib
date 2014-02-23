#include <stdlib.h>
#include <stdio.h>

#include "rbtree.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct user_def_type{
	struct rb_node rb_node;
	int key;
}user_def_type;


struct user_def_type *my_rb_find(struct rb_root *root, int num)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct user_def_type *data = container_of(node, struct user_def_type, rb_node);

		if (num < data->key)
			node = node->rb_left;
		else if (num > data->key)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}


int my_rb_insert_unique(struct rb_root *root, struct user_def_type *data)
{
	struct rb_node **new_node = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new_node) {
		struct user_def_type *this_node = container_of(*new_node, struct user_def_type, rb_node);

		parent = *new_node;
		if (data->key < this_node->key)
			new_node = &((*new_node)->rb_left);
		else if (data->key > this_node->key)
			new_node = &((*new_node)->rb_right);
		else
			return FALSE;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->rb_node, parent, new_node);
	rb_insert_color(&data->rb_node, root);

	return TRUE;
}


int my_rb_insert_equal(struct rb_root *root, struct user_def_type *data)
{
	struct rb_node **new_node = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new_node) {
		struct user_def_type *this_node = container_of(*new_node, struct user_def_type, rb_node);

		parent = *new_node;
		if (data->key < this_node->key)
			new_node = &((*new_node)->rb_left);
		else
			new_node = &((*new_node)->rb_right);
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->rb_node, parent, new_node);
	rb_insert_color(&data->rb_node, root);

	return TRUE;
}


void my_rb_delete(struct rb_root *root, int key)
{
	struct user_def_type *data = my_rb_find(root, key);

	if (!data) {
		fprintf(stderr, "Not found %d.\n", key);
		return;
	}

	rb_erase(&data->rb_node, root);
	free(data);
}


void print_rbtree(struct rb_root *tree)
{
	struct rb_node *node;
	int is_empty_tree = TRUE;

	printf("Current rbtree: ");
	for (node = rb_first(tree); node; node = rb_next(node))
	{
		is_empty_tree = FALSE;
		printf("%d ", rb_entry(node, struct user_def_type, rb_node)->key);
	}

	if (is_empty_tree)
	{
		printf("empty");
	}
	printf("\n");
}


/* clear the whole rbtree */
void my_rbtree_clear(struct rb_root *tree)
{
	struct rb_node *pre_node, *node;
	struct user_def_type *data;

	node = rb_first(tree);
	while (node)
	{
		pre_node = node;
		node = rb_next(node);

		rb_erase(pre_node, tree);
		data = container_of(pre_node, struct user_def_type, rb_node);
		/* printf("free key: %d\n", data->key); */
		free(data);
	}
}


void rbtree_handle_and_del(struct rb_root *tree)
{
	struct rb_node *pre_node, *node;
	struct user_def_type *data;

	node = rb_first(tree);
	while (node)
	{
		pre_node = node;
		node = rb_next(node);

		printf("Handle and delete key: %d\n", rb_entry(pre_node, struct user_def_type, rb_node)->key);
		rb_erase(pre_node, tree);
		data = container_of(pre_node, struct user_def_type, rb_node);
		free(data);

		print_rbtree(tree);
	}
	printf("\n");
}


void init_rbtree_test_data(struct rb_root *tree)
{
	int i, ret;
	int test_key[10] = {2, 300, 100, 300, 365, 30, 300, 250, 310, 210};
	struct user_def_type *tmp;

	for (i = 0; i < sizeof(test_key)/sizeof(test_key[0]); i++) {
	tmp = malloc(sizeof(struct user_def_type));
	if (!tmp)
		perror("Allocate dynamic memory");

	tmp->key = test_key[i];

	ret = my_rb_insert_equal(tree, tmp);
	/* if (ret < 0) {
		fprintf(stderr, "The %d already exists.\n", tmp->key);
		free(tmp);
	} */
	}
}


int main(int argc, char** argv)
{
	struct rb_root my_rb_tree;
	my_rb_tree.rb_node = NULL;

	/* rbtree_handle_and_del test */
	init_rbtree_test_data(&my_rb_tree);
	print_rbtree(&my_rb_tree);
	printf("\n\n");
	rbtree_handle_and_del(&my_rb_tree);

	/* clear tree test */
	init_rbtree_test_data(&my_rb_tree);
	print_rbtree(&my_rb_tree);
	my_rbtree_clear(&my_rb_tree);
	print_rbtree(&my_rb_tree);

	return 0;
}
