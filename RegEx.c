// match XML regular expression 
// 10 MAR 2011

// compile as standalone application

// #define STANDALONE

// author: karl malbrain, malbrain@yahoo.com

/*
This work, including the source code, documentation
and related data, is placed into the public domain.

The orginal author is Karl Malbrain.

THIS SOFTWARE IS PROVIDED AS-IS WITHOUT WARRANTY
OF ANY KIND, NOT EVEN THE IMPLIED WARRANTY OF
MERCHANTABILITY. THE AUTHOR OF THIS SOFTWARE,
ASSUMES _NO_ RESPONSIBILITY FOR ANY CONSEQUENCE
RESULTING FROM THE USE, MODIFICATION, OR
REDISTRIBUTION OF THIS SOFTWARE.
*/

#include <stdlib.h>
#include <memory.h>
#include <stdint.h>
#include <string.h>

#ifdef STANDALONE
#include <stdio.h>
typedef unsigned char uchar;
#endif

//	definition of parser nodes placed in the bottom
//	of the expression work area after the base data.

struct Node {
	int typelen;			// type/len of pattern
	int minimum;			// minimum number of matches
	int maximum;			// maximum pattern matches
	union {
		struct Node *child;	// child pointer
		uchar *pattern;		// pattern pointer
	} type[1];
	struct Node *parent;	// parent node
	struct Node *ornode;	// last "or" node
	struct Node *next;		// sibling node
};

//	definition of nfa evaluation nodes placed at the top
//	of the expression work area

struct Probe {
	struct Node *node;		// node probe is currently on
	int occurrence;			// occurrence of node
	int stack;				// expression descent stack
	int next;				// next index of probe in list
	int off;				// offset of the input string
};

//	base data of the expression work area

struct Expr {
	int size;		// size of structure
	int amt;		// length of value
	int top;		// next probe allocator
	int dead;		// dead probe index chain
	int tree;		// size of tree node area
	int steps;		// number of evaluation steps
	uchar *val;		// value to search over
	uchar *memo;	// evaluator memo array
};

//	make new probe (or new descent stack node)
//	from top of work area

struct Probe *regprobe (struct Expr *expr)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
struct Probe *probe;

	if( expr->dead ) {
		probe = base - expr->dead;
		expr->dead = probe->next;
	} else
		probe = base - ++expr->top;

	//	out of memory??

	if( expr->tree + sizeof(struct Expr) > expr->size - expr->top * sizeof(*probe) )
		return expr->top--, NULL;

	memset (probe, 0, sizeof(*probe));
	return probe;
}

//	clone probe to continue with tree node siblings or children

struct Probe *regclone (struct Expr *expr, struct Probe *probe)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
struct Probe *clone, *oldstack, *stack, *prevstack;
int nxt, prev = 0;

	if( clone = regprobe (expr) )
		clone->node = probe->node;
	else
		return NULL;

	clone->off = probe->off;

	//	clone the expression stack

	if( nxt = probe->stack ) do {
		oldstack = base - nxt;
		if( stack = regprobe (expr) )
			*stack = *oldstack;
		else
			return NULL;
		if( prev ) {
			prevstack = base - prev;
			prevstack->next = (uint32_t)(base - stack);
		} else
			clone->stack = (uint32_t)(base - stack);
		prev = (uint32_t)(base - stack);
	} while( nxt = oldstack->next );

	return clone;
}

//	allocate new expression node

struct Node *regnode (struct Expr *expr)
{
uchar *next = (uchar *)(expr + 1);

	//	allocate tree node

	next += expr->tree;
	expr->tree += sizeof(struct Node);

	if( expr->tree + sizeof(struct Expr) > expr->size - expr->top * sizeof(struct Probe) )
		return NULL;

	memset (next, 0, sizeof(struct Node));
	return (struct Node *)next;
}

//	construct special pattern node from escaped type

struct Node *regspcl (struct Expr *expr, uchar *type, struct Node *parent)
{
struct Node *node;
uchar *pat;

	switch( *type ) {
	case 'c':	pat = "[-._:A-Za-z0-9]";	break;
	case 'C':	pat = "[^-._:A-Za-z0-9]";	break;
	case 'd':	pat = "[0-9]";	break;
	case 'D':	pat = "[^0-9]";	break;
	case 's':	pat = "[ 	]";	break;
	case 'S':	pat = "[^ 	]";	break;
	case 'i':	pat = "[a-zA-Z_:]";		break;
	case 'I':	pat = "[^a-zA-Z_:]";	break;
	default:

	  // pattern is escaped regular character

	  if( node = regnode (expr) ) {
		node->maximum = 1;
		node->minimum = 1;
		node->typelen = 1;
		node->type->pattern = type;
		node->parent = parent;
	  }
	  return node;
	}

	if( node = regnode (expr) ) {
		node->maximum = 1;
		node->minimum = 1;
		node->typelen = (uint32_t)strlen(pat);
		node->type->pattern = pat;
		node->parent = parent;
	}
	return node;
}

//	construct new pattern node

struct Node *regpat (struct Expr *expr, uchar *pat, int len, struct Node *parent)
{
struct Node *node;

	if( node = regnode (expr) ) {
		node->maximum = 1;
		node->minimum = 1;
		node->typelen = len;
		node->parent = parent;
		node->type->pattern = pat;
	}
	return node;
}

//	append node at end of chain

void regappend (struct Node *node, struct Node *prev)
{
struct Node *parent;

	if( !prev ) {
		parent = node->parent;
		parent->type->child = node;
		return;
	}

	prev->next = node;
}

//	capture mini-max specification

int regminimax (struct Expr *expr, uchar *pat, int max, struct Node *node)
{
int comma = 0, ch;
int sum = 0;
int len = 1;

	node->minimum = 0;
	node->maximum = 0;

	while( len < max ) {
		switch( ch = pat[len++] ) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			sum *= 10;
			sum += ch & 0xf;
			continue;
		case ',':
			if( !comma++ ) {
				node->minimum = sum;
				sum = 0;
				if( pat[len] == '}' ) {
					node->maximum = 0x7fffffff;
					return len + 1;
				}
			}
			continue;
		case '}' :
			if( !comma ) {
				node->minimum = sum;
				node->maximum = sum;
			} else
				node->maximum = sum;
			return len;
		}
	}
	return len;
}

//	compile regular expression
//	return Expr object

int regcomp (struct Expr *expr, int size, uchar *pat, int len)
{
int bnest = 0, off = 0, ch;
struct Node *node = NULL;
struct Node *prev = NULL;
struct Node *parent;

  if( size < sizeof(*expr) )
	return 0;

  memset (expr, 0, sizeof(*expr));
  expr->size = size;

  if( parent = regpat(expr, NULL, 0, NULL) )
	while( off < len ) {
	  switch( ch = pat[off] ) {
	  case '{':
		if( node ) {
			off += regminimax (expr, pat + off, len - off, node);
			continue;
		}
		return 0;

	  case ']':
		return 0;

	  case '[':
		if( node = regpat(expr, pat + off++, 1, parent) )
			bnest = 1;
		else
			return 0;

		while( off < len && bnest )
			if( pat[off] == '[' )
				bnest++, off++, node->typelen++;
			else if( pat[off] == ']' )
				--bnest, off++, node->typelen++;
			else
				off++, node->typelen++;

		regappend (node, prev);
		prev = node;
		continue;

	  // "or" node

	  case '|':
		if( node = regnode(expr) ) {
			node->typelen = -1;
			node->parent = parent;
			node->minimum = 0;
			node->maximum = 1;
		} else
			return 0;


		//	if already underway,
		//	move node chain under
		//	new "or" node

		if( parent->ornode ) {
			node->type->child = parent->ornode->next;
			parent->ornode->next = node;
		} else {
			node->type->child = parent->type->child;
			parent->type->child = node;
		}

		parent->ornode = prev = node;

		//	reparent child nodes
		//	under new "or" node

		if( node = node->type->child )
		  do node->parent = prev;
		  while( node = node->next );

		off++;
		continue;

	  case '(':
		if( parent = regpat (expr, NULL, 0, parent) ) // expression node
			regappend (parent, prev);
		else
			return 0;
		prev = node = NULL;
		off++;
		continue;

	  case ')':
		if( node = parent ) {
			off++;
			parent = node->parent;
			if( prev = parent->type->child )
				while( prev->next )
					prev = prev->next;
			
			continue;
		}
		return 0;

	  case '\\':
		off++;
		if( pat[off] >= 'A' && pat[off] <= 'Z' || pat[off] >= 'a' && pat[off] <= 'z' )
		  if( node = regspcl(expr, pat + off, parent) ) {
			regappend (node, prev);
			prev = node;
			off++;
			continue;
		  }
			
	  default:
		if( node = regpat(expr, pat + off++, 1, parent) )
			regappend (node, prev);
		else
			return 0;

		prev = node;
		continue;

	  case '?':
		if( node ) {
			node->minimum = 0;
			node->maximum = 1;
			off++;
			continue;
		}
		return 0;

	  case '+':
		if( node ) {
			node->minimum = 1;
			node->maximum = 0x7fffffff;
			off++;
			continue;
		}
		return 0;

	  case '*':
		if( node ) {
			node->minimum = 0;
			node->maximum = 0x7fffffff;
			off++;
			continue;
		}
		return 0;
	  }
	}

	return 1;
}

int regfilter (uchar *pat, int patlen, uchar tst);

//	filter escaped character

int regfilterspcl (uchar ch, uchar tst)
{
	switch( ch ) {
	case 'd':	return regfilter ("[0-9]", 5, tst);
	case 'D':	return regfilter ("[^0-9]", 6, tst);
	case 's':	return regfilter ("[ 	]", 2, tst);
	case 'S':	return regfilter ("[^ 	]", 3, tst);
	case 'i':	return regfilter ("[a-zA-Z_:]", 8, tst);
	case 'I':	return regfilter ("[^a-zA-Z_:]", 9, tst);
	}

	return regfilter (&ch, 1, tst);
}

//	filter character against specifier pattern

int regfilter (uchar *pat, int patlen, uchar tst)
{
int found = 0;
int strip = 0;
int prev, ch;
int mode = 0;
int len = 0;
int not = 0;
int expr;

  while( len < patlen )
	switch( ch = pat[len++] ) {
	case '\\':
		if( len < patlen )
			ch = pat[len++];
		else
			continue;
		
		if( ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' ) {
		  expr = regfilterspcl (ch, tst);

		  if( !not && expr || not && !expr )
			if( !mode )
				found = 1;
			else if( mode & 1 )
				strip = 1;
			else if( ~mode & 1 )
				strip = 0;
		  continue;
		}

	default:
		expr = ch == tst;
		prev = ch;

		if( len < patlen && pat[len] == '-' )
			continue;

		if( !not && expr || not && !expr )
			if( !mode )
				found = 1;
			else if( mode & 1 )
				strip = 1;
			else if( ~mode & 1 )
				strip = 0;
		continue;
	case '^':
		not++;
		continue;
	case '-':
		if( len < patlen && pat[len] == '[' ) {
			mode++;
			len++;
			continue;
		}
		if( len < patlen )
			expr = tst >= prev && tst <= pat[len];
		else
			expr = 0;

		if( !not && expr || not && !expr )
			if( !mode )
				found = 1;
			else if( (mode & 1) )
				strip = 1;
			else if( (~mode & 1) )
				strip = 0;
		len++;
		continue;

	case ']':
		if( !mode-- )
			break;
		continue;
	}

	if( strip )
		found = 0;

	return found;
}

//	move probe to next node, or pop expression
//	or return 0 at end of tree

int regnext (struct Expr *expr, struct Probe *probe)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
struct Probe *stack;

	if( probe->node->next ) {
		probe->node = probe->node->next;
		probe->occurrence = 0;
		return 1;
	}

	while( probe->stack ) {
		stack = base - probe->stack;
		probe->stack = stack->next;
		probe->occurrence = stack->occurrence;

		//	add to free list

		stack->next = expr->dead;
		expr->dead = (uint32_t)(base - stack);

		// 	move to parent node in tree

		if( probe->node = probe->node->parent )
		  if( probe->node->typelen < 0 )
			continue;
		  else
			return 1;
	}

	return 0;
}

//	match next character of input against pattern node
//	return 1 if match, 0

int regmatch (struct Expr *expr, struct Probe *probe)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
int ch;

	if( probe->off == expr->amt )
		return 0;

	// match pattern

	switch( ch = *probe->node->type->pattern ) {
	case '.':
		return 1;

	default:
		if( expr->val[probe->off] == ch )
			return 1;

		return 0;

	case '[':
		if( probe->node->typelen > 2 )
		  if( regfilter (probe->node->type->pattern + 1, probe->node->typelen - 2, expr->val[probe->off]) )
			return 1;
		return 0;
	}
}

//	return probe to free list

void regkill (struct Expr *expr, struct Probe *probe)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
int idx, nxt;

	probe->next = expr->dead;
	expr->dead = (uint32_t)(base - probe);

	//	kill expression stack

	if( idx = probe->stack ) do {
		probe = base - idx;
		nxt = probe->next;
		probe->next = expr->dead;
		expr->dead = idx;
	} while( idx = nxt );
}

//	evaluate expression tree by racing tree probes 
//	across the input string

int regevaluate (struct Expr *expr, uchar *val, int amt)
{
struct Probe *base = (struct Probe *)((uchar *)expr + expr->size);
struct Probe *probe, *stack, *clone;
int idx, queue;

	//	reset evaluator

	expr->val = val;
	expr->amt = amt;
	expr->top = 0;

	//	if using new compiled node tree

	if( !expr->memo )
		expr->memo = (uchar *)(expr + 1) + expr->tree;

	//	calculate size of memo array in bits
	//	by calculating number of nodes
	//	and multiplying by source len

	idx = (uint32_t)((expr->memo - (uchar *)(expr + 1)) / sizeof(struct Node));

	//	convert number of bits to number of bytes
	//	and clear memo array

	idx = (idx * (amt + 1) + 7) / 8;
	expr->tree = idx + (uint32_t)(expr->memo - (uchar *)(expr + 1));

	if( expr->tree + (int)sizeof(struct Expr) > expr->size )
		return 0;	// out of memory
	else
		memset (expr->memo, 0, idx);

	//	launch initial probe on root of parse tree

	if( probe = regprobe (expr) )
		probe->node = (struct Node *)(expr + 1);
	else
		return 0;	// out of memory

	queue = (uint32_t)(base - probe);

	//	evaluate input string against parse tree
	//	until a probe reaches both the end of the
	//	parse tree and the end of the input string

	while( idx = queue ) {
	  probe = base - idx;
	  queue = probe->next;

	  //	continue our node down to a
	  //	pattern match node.

	  while( ++expr->steps ) {
		//	if maximum occurrences reached
		//	move to sibling node
		//	if no sibling, either return
		//	success if done, or kill probe

		if( probe->occurrence == probe->node->maximum )
			if( regnext (expr, probe) )
				continue;
			else if( probe->off == expr->amt )
				return 1;
			else
				break;

		//	if another probe began evaluation
		//	of this node at this offset before,
		//	abandon our probe.

		idx = (uint32_t)(probe->node - (struct Node *)(expr + 1));
		idx *= amt + 1;
		idx += probe->off;

		if( ++probe->occurrence > probe->node->minimum )
		  if( expr->memo[idx/8] & (1 << (idx % 8)) )
			break;
	 	  else
	 		expr->memo[idx/8] |= 1 << (idx % 8);

		//	if minimum requirement met
		//	clone another probe to continue
		//	with alternate

		if( probe->occurrence > probe->node->minimum )
		  if( clone = regclone (expr, probe) ) {
			clone->occurrence = clone->node->maximum;
			clone->next = queue;
			queue = (uint32_t)(base - clone);
		  } else
			return 0;		//	out of memory

		// descend probe into subexpressions

		if( probe->node->typelen <= 0 ) {

			//	make a stack node
			//	to remember parent

			if( stack = regprobe (expr) )
				stack->next = probe->stack;
			else
				return 0;	// out of memory

			stack->occurrence = probe->occurrence;
			stack->off = probe->off;

			probe->node = probe->node->type->child;
			probe->stack = (uint32_t)(base - stack);
			probe->occurrence = 0;
			continue;
		}

		//	advance to next input character,
		//	or kill probe if no pattern match,

		if( regmatch (expr, probe) )
			probe->off++;
		else
			break;
	  }

	//	delete our probe and continue
	//	with next queued clone

	regkill (expr, probe);
	}

	//	when run queue is exhausted,
	//	delete all probes and return failure

	return 0;
}

#ifndef STANDALONE

//	RegExpr:  pattern match buffer with argument
//		and set TRUE on match

bool RegExpr (int size, char *pattern, int patLen, char *value, int valLen)
{
void *expr;
bool flag;

	if( !size )
		size = 32768;

	if( size > sizeof(struct Expr) )
		expr = malloc (size);
	else {
    fprintf(stderr, "working area too small\n");
		exit(1);
  }

	if( !regcomp(expr, size, arg->buff, arg->len) ) {
    fprintf(stderr, "XSD pattern compile error\n");
    free(expr);
		exit(1);
  }
  
	flag = regevaluate (expr, val, valLen);
	free(expr);

	return flag;
}
#endif

#ifdef STANDALONE

int main (int argc, uchar **args)
{
struct Expr *expr;
int size;

	if( argc < 3 ) {
		fprintf (stderr, "Usage: regexpr value expression\n");
		return 1;
	}
	if( argc > 3 )
		size = atoi(args[3]);
	else
		size = 32768;

	expr = malloc (size);
	expr->val = args[1];
	expr->amt = (uint32_t)strlen (args[1]);

	if( !regcomp(expr, size, args[2], (uint32_t)strlen(args[2])) )
		fprintf (stderr, "Pattern compilation error\n");
	else if( regevaluate (expr, args[1], (uint32_t)strlen(args[1])) )
		fprintf (stderr, "Match in %d steps\n", expr->steps);
	else
		fprintf (stderr, "No match in %d steps\n", expr->steps);

	return 0;
}
#endif
