/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This file defines RB tree functions.
*/

#include "rb.h"

// Cohort lock for the whole tree
extern argo::globallock::cohort_lock* lock_1;

void Red_Black_Tree::left_rotation(Node* x) {
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(x->right, sizeof(Node));
		argo::backend::selective_acquire((x->right)->left, sizeof(Node));
	#endif
	Node* y = x->right;
	recordChange(x);
	x->right = y->left;
	if (y->left) {
		recordChange(y->left);
		y->left->parent = x;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(y->left, sizeof(Node));
		#endif
	}

	y->parent = x->parent;

	if (x == getRoot()) {
		changeRoot(y);
	} else if (x == x->parent->left){
		recordChange(x->parent);
		x->parent->left = y;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(x->parent, sizeof(Node));
		#endif
	} else {
		recordChange(x->parent);
		x->parent->right = y;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(x->parent, sizeof(Node));
		#endif
	}

	recordChange(y);
	y->left = x;

	recordChange(x);
	x->parent = y;
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(x, sizeof(Node));
		argo::backend::selective_release(y, sizeof(Node));
	#endif
}

void Red_Black_Tree::right_rotation(Node* x) { //todo
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(x->left, sizeof(Node));
		argo::backend::selective_acquire((x->left)->right,sizeof(Node));
		argo::backend::selective_acquire(x->parent,sizeof(Node));
	#endif
	Node* y = x->left;
	recordChange(x);
	x->left = y->right;
	if (y->right) {
		recordChange(y->right);
		y->right->parent = x;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release((y->right), sizeof(Node));
		#endif
	}

	y->parent = x->parent;

	if (x == getRoot()) {
		changeRoot(y);
	} else if (x == x->parent->right) {
		recordChange(x->parent);
		x->parent->right = y;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(x->parent, sizeof(Node));
		#endif
	} else {
		recordChange(x->parent);
		x->parent->left = y;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(x->parent, sizeof(Node));
		#endif
	}

	recordChange(y);
	y->right = x;
	recordChange(x);
	x->parent = y;
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(y, sizeof(Node));
		argo::backend::selective_release(x, sizeof(Node));
	#endif
}

void Red_Black_Tree::rb_insert(int val) {
	Node* z = createNode(val);
	Node* y = NULL;
	Node* x = getRoot();

	while (x) {
		y = x;
		if (z->val < x->val) {
			x = x->left;
		} else {
			x = x->right;
		}
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(x, sizeof(Node));
		#endif
	}

	z->parent = y;

	if (!y) {
		changeRoot(z);
	} else if (z->val < y->val) {
		recordChange(y);
		y->left = z;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(y, sizeof(Node));
		#endif
	} else {
		recordChange(y);
		y->right = z;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(y, sizeof(Node));
		#endif
	}

	recordChange(z);
	z->color = RED;
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(z, sizeof(Node));
	#endif
	insert_fix_up(z);
}

void Red_Black_Tree::rb_insert(Node* z) {//todo selective acq/rel?
	Node* y = NULL;
	Node* x = getRoot();

	while (x) {
		y = x;
		if (z->val < x->val) {
			x = x->left;
		} else {
			x = x->right;
		}
	}

	z->parent = y;

	if (!y) {
		changeRoot(z);
	} else if (z->val < y->val) {
		recordChange(y);
		y->left = z;
	} else {
		recordChange(y);
		y->right = z;
	}

	recordChange(z);
	z->color = RED;

	insert_fix_up(z);
}

void Red_Black_Tree::insert_fix_up(Node* z) {
	Node* y = NULL;
	Node* gp, *p;
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(z->parent, sizeof(Node));
	#endif
	while (z->parent && z->parent->color == RED) {
		p = z->parent;
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(p->parent, sizeof(Node));
		#endif
		gp = p->parent;
		if(!gp) break;
		if (p == gp->left) {
			y = gp->right;
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(gp->right, sizeof(Node));
			#endif
			if (y && y->color == RED) {
				recordChange(p);
				p->color = BLACK;
				recordChange(y);
				y->color = BLACK;
				recordChange(gp);
				gp->color = RED;
				recordChange(z);
				z = gp;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(p, sizeof(Node));
					argo::backend::selective_release(y, sizeof(Node));
					//argo::backend::selective_release(gp, sizeof(Node));
					argo::backend::selective_release(z, sizeof(Node));
				#endif
				continue;
			}
			if (z == p->right) {
				//Left-right case
				Node* tmp;
				left_rotation(p);
				recordChange(z);
				recordChange(p);
				tmp = p;
				p = z;
				z = tmp;
			}
			//left-left case
			recordChange(p);
			recordChange(gp);
			p->color = BLACK;
			gp->color = RED;
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(p, sizeof(Node));
				argo::backend::selective_release(gp, sizeof(Node));
			#endif
			right_rotation(gp);
		} else {
			#if SELECTIVE_ACQREL
					argo::backend::selective_acquire(gp->left, sizeof(Node));
				#endif
			y = gp->left;
			if (y && y->color == RED) {
				recordChange(p);
				p->color = BLACK;
				recordChange(y);
				y->color = BLACK;
				recordChange(gp);
				gp->color = RED;
				recordChange(z);
				z = gp;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(p, sizeof(Node));
					argo::backend::selective_release(y, sizeof(Node));
					argo::backend::selective_release(z, sizeof(Node));
					argo::backend::selective_release(gp, sizeof(Node));
				#endif
				continue;
			}
			if (z == p->left) {
				//Right-left case
				Node* tmp;
				right_rotation(p);
				recordChange(z);
				recordChange(p);
				tmp = p;
				p = z;
				z = tmp;
			}
			//Right-right case
			recordChange(p);
			recordChange(gp);
			p->color = BLACK;
			gp->color = RED;
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(p, sizeof(Node));
				argo::backend::selective_release(gp, sizeof(Node));
			#endif
			left_rotation(gp);
		}
	}
	recordChange(getRoot());
	getRoot()->color = BLACK;
	Node* placeholder = getRoot();
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(placeholder, sizeof(Node));
	#endif
}

Node* Red_Black_Tree::rb_search(int val) { //todo selective acq/rel on current?
	Node* current = getRoot();
	while (current) {
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(current, sizeof(Node));
		#endif
		if (val == current->val) {
			return current;
		} else if (val < current->val) {
			current = current->left;
		} else {
			current = current->right;
		}
	}
	return current;
}

Node* Red_Black_Tree::successor(Node* x) {//changed
	Node* current = NULL;
	if (x->right) {
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(x->right, sizeof(Node));
		#endif
		current = x->right;
		while (current->left) {
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(current->left, sizeof(Node));
			#endif
			current = current->left;
		}
	}
	return current;
}

void Red_Black_Tree::rb_delete(Node* z) { //todo selective acq/rel?
	Node *x, *y;
	Color color;
	Node* succ;
	if (z->left && z->right) {
		succ = successor(z);
		if (z != getRoot()) {
			recordChange(z->parent);//todo blank??
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(z->parent, sizeof(Node));
			#endif
			if (z->parent->left == z){
				z->parent->left = succ;
			} else {
				z->parent->right = succ;
			}
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(z->parent, sizeof(Node));
			#endif
		} else {
			changeRoot(succ);
		}
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(succ->right, sizeof(Node));
			argo::backend::selective_acquire(succ->parent, sizeof(Node));
		#endif
		x = succ->right;
		y = succ->parent;
		color = succ->color;

		if (z == y) {
			y = succ;
		} else {
			if (x) {
				recordChange(x);
				x->parent = y;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(x, sizeof(Node));
				#endif
			}
			recordChange(y);
			y->left = x;
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(z->right, sizeof(Node));
			#endif
			recordChange(succ->right);
			succ->right = z->right;
			recordChange(z->right);
			z->right->parent = succ;
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(y, sizeof(Node));
				argo::backend::selective_release(succ, sizeof(Node));
				argo::backend::selective_release(z->right, sizeof(Node));
			#endif
		}

		recordChange(succ);
		succ->parent = z->parent;
		succ->color = z->color;
		succ->left = z->left;
		recordChange(z->left);
		z->left->parent = succ;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(z->left, sizeof(Node));
			argo::backend::selective_release(succ, sizeof(Node));
		#endif
		if (color == BLACK) {
			delete_fix_up(x, y);
		}
		//delete z
		recordChange(z);
		z->val = 0;
		z->left = NULL;
		z->right = NULL;
		z->parent = NULL;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(z, sizeof(Node));
		#endif
		//        nvm_free(z);
	} else {
		if (z->left) {
			x = z->left;
		} else {
			x = z->right;
		}
		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(x, sizeof(Node));
			argo::backend::selective_acquire(z->parent, sizeof(Node));
		#endif
		y = z->parent;
		color = z->color;
		if (x) {
			recordChange(x);
			x->parent = y;
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(x, sizeof(Node));
			#endif
		}

		if (y) {
			recordChange(y);
			if (y->left == z) {
				y->left = x;
			} else {
				y->right = x;
			}
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(y, sizeof(Node));
			#endif
		} else {
			changeRoot(x);
		}
		if (color == BLACK) {
			delete_fix_up(x, y);
		}
		//delete z
		recordChange(z);
		z->val = 0;
		z->left = NULL;
		z->right = NULL;
		z->parent = NULL;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(z, sizeof(Node));
		#endif
		//        nvm_free(z);
	}
}
//todo y might need to be invalidated
void Red_Black_Tree::delete_fix_up(Node* x, Node* y) { //#todo
	Node* w;
	while ((!x || x->color == BLACK) && x != getRoot()) {
		if (x == y->left) {
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(y->right, sizeof(Node));
			#endif
			w = y->right;
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(w->left, sizeof(Node));
				argo::backend::selective_acquire(w->right, sizeof(Node));
			#endif
			if (w->color == RED) {
				recordChange(w);
				w->color = BLACK;
				recordChange(y);
				y->color = RED;
				left_rotation(y);
				w = y->right; //todo redundant??
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));//todo correct?
					argo::backend::selective_release(y, sizeof(Node));
				#endif
			}
			else if ((!w->left || w->left->color == BLACK) &&
					(!w->right || w->right->color == BLACK)) {
				recordChange(w);
				w->color = RED;

				if(y->color == RED) {
					recordChange(y);
					y->color = BLACK;
					#if SELECTIVE_ACQREL
						argo::backend::selective_release(y, sizeof(Node));
						argo::backend::selective_release(w,sizeof(Node));
					#endif
					break;
				}

				x = y;
				#if SELECTIVE_ACQREL
					argo::backend::selective_acquire(x->parent, sizeof(Node));
				#endif
				y = x->parent;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));
				#endif
			} else {
				if (!w->right || w->right->color == BLACK) {
					recordChange(w->left);
					w->left->color = BLACK;
					recordChange(w);
					w->color = RED;
					#if SELECTIVE_ACQREL
						argo::backend::selective_release(w->left, sizeof(Node));
					#endif
					right_rotation(w);
					w = y->right;
				}
				recordChange(w);
				w->color = y->color;
				recordChange(y);
				y->color = BLACK;
				recordChange(w->right);
				w->right->color = BLACK;
				left_rotation(y); //todo fix
				x = getRoot();
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));
					argo::backend::selective_release(y, sizeof(Node));
					argo::backend::selective_release(w->right, sizeof(Node));
				#endif
				break;
			}
		} else {
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(y->left, sizeof(Node));
			#endif
			w = y->left;
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(w->left, sizeof(Node));
				argo::backend::selective_release(w->right, sizeof(Node));
			#endif
			if (w->color == RED) {
				recordChange(w);
				w->color = BLACK;
				recordChange(y);
				y->color = RED;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));
					argo::backend::selective_release(y, sizeof(Node));
				#endif
				right_rotation(y);
				w = y->left;
			}
			else if ((!w->left || w->left->color == BLACK) &&
					(!w->right || w->right->color == BLACK)) {
				recordChange(w);
				w->color = RED;

				if(y->color == RED) {
					recordChange(y);
					y->color = BLACK;
					#if SELECTIVE_ACQREL
						argo::backend::selective_release(w, sizeof(Node));
						argo::backend::selective_release(y, sizeof(Node));
					#endif
					break;
				}

				//Y is black, W is black and its children are black
				x = y;
				y = x->parent;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));
				#endif
			} else {
				if (!w->left || w->left->color == BLACK) {
					recordChange(w->right);
					w->right->color = BLACK;
					recordChange(w);
					w->color = RED;
					#if SELECTIVE_ACQREL
						argo::backend::selective_release(w->right, sizeof(Node));
						argo::backend::selective_release(w, sizeof(Node));
					#endif
					left_rotation(w);
					w = y->left;
				}
				recordChange(w);
				w->color = y->color;
				recordChange(y);
				y->color = BLACK;
				recordChange(w->left);
				w->left->color = BLACK;
				#if SELECTIVE_ACQREL
					argo::backend::selective_release(w, sizeof(Node));
					argo::backend::selective_release(y, sizeof(Node));
					argo::backend::selective_release(w->left, sizeof(Node));
				#endif
				right_rotation(y);
				x = getRoot();
				break;
			}
		}
	}
	if (x) {
		recordChange(x);
		x->color = BLACK;
		#if SELECTIVE_ACQREL
			argo::backend::selective_release(x, sizeof(Node));
		#endif
	}
}

bool Red_Black_Tree::rb_delete_or_insert(int num_updates) {
	lock(); //todo change to selective!!

	for(int i = 0; i < num_updates; i++) {
		int val =  rand() % (tree_length);
		Node* toFind = rb_search(val);

		#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(toFind, sizeof(Node));
		#endif
		if (toFind) {
			rb_delete(toFind);
		}
		else {
			rb_insert(val);
		}
	}
	unlock();
	return true;
}

Red_Black_Tree::Red_Black_Tree(Node* root) {
	// initialize start_ptr
	start_1 = root;
	// initialize end ptr
	tree_1_end = root_1;
}

void Red_Black_Tree::initialize(Node* root, int* array, unsigned length) {
	// initialize start_ptr
	start_1 = root;

	// initialize end ptr
	tree_1_end = root;

	// build tree_1
	for (unsigned i = 0; i < length; ++i) {
		Node* new_node = tree_1_end;
		*new_node = Node(array[i]);
		tree_1_end++;
		rb_insert(new_node);
	}

	tree_length = length;
	//init mutex
	// lock_1 = argo::new_<argo::globallock::cohort_lock>();
	return;
}

Red_Black_Tree::Red_Black_Tree(Node* root, int* array, unsigned length) {
	// initialize start_ptr
	start_1 = root;

	// initialize end ptr
	tree_1_end = root;

	// build tree_1
	for (unsigned i = 0; i < length; ++i) {
		Node* new_node = tree_1_end;
		new_node->val = array[i];
		tree_1_end++;
		rb_insert(new_node);
	}

	tree_length = length;
	//init mutex
	// lock_1 = argo::new_<argo::globallock::cohort_lock>();
}

Node* Red_Black_Tree::createNode(int _val) {
	Node* new_node_1 = start_1 + _val;
	assert(!new_node_1->left && !new_node_1->right && !new_node_1->parent);
	*new_node_1 = Node(_val);
	return new_node_1;
}

Node* Red_Black_Tree::getRoot() { //todo selective acq/rel on root?
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(root_1, sizeof(Node));
	#endif
	return root_1;
}

void Red_Black_Tree::changeRoot(Node* x) {
	recordChange(root_1);
	root_1 = x;
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(root_1, sizeof(Node));
	#endif
}

void Red_Black_Tree::recordChange(Node* node) {
	return;
}

void Red_Black_Tree::clearChange() {
	changed_nodes.clear();
}

void Red_Black_Tree::copy_changes() {
	return;
}

void Red_Black_Tree::lock() {//todo alias for locking. remove
	lock_1->lock();
}

void Red_Black_Tree::unlock() {
	lock_1->unlock();
}
