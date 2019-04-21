/*
 *  apfsprogs/apfsck/btree.h
 *
 * Copyright (C) 2019 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#ifndef _BTREE_H
#define _BTREE_H

#include "object.h"
#include "types.h"

struct super_block;
struct extref_record;

/* Flags for the object map structure */
#define APFS_OMAP_MANUALLY_MANAGED	0x00000001
#define APFS_OMAP_ENCRYPTING		0x00000002
#define APFS_OMAP_DECRYPTING		0x00000004
#define APFS_OMAP_KEYROLLING		0x00000008
#define APFS_OMAP_CRYPTO_GENERATION	0x00000010
#define APFS_OMAP_FLAGS_VALID_MASK	(APFS_OMAP_MANUALLY_MANAGED \
					| APFS_OMAP_ENCRYPTING \
					| APFS_OMAP_DECRYPTING \
					| APFS_OMAP_KEYROLLING \
					| APFS_OMAP_CRYPTO_GENERATION)

/*
 * On-disk representation of an object map
 */
struct apfs_omap_phys {
/*00*/	struct apfs_obj_phys om_o;
/*20*/	__le32 om_flags;
	__le32 om_snap_count;
	__le32 om_tree_type;
	__le32 om_snapshot_tree_type;
/*30*/	__le64 om_tree_oid;
	__le64 om_snapshot_tree_oid;
/*40*/	__le64 om_most_recent_snap;
	__le64 om_pending_revert_min;
	__le64 om_pending_revert_max;
} __packed;

/*
 * Structure of a value in an object map B-tree
 */
struct apfs_omap_val {
	__le32 ov_flags;
	__le32 ov_size;
	__le64 ov_paddr;
} __packed;

/*
 * Object map record data in memory
 */
struct omap_record {
	u64 bno;
	u64 xid;
};

/* B-tree node flags */
#define APFS_BTNODE_ROOT		0x0001
#define APFS_BTNODE_LEAF		0x0002
#define APFS_BTNODE_FIXED_KV_SIZE	0x0004
#define APFS_BTNODE_CHECK_KOFF_INVAL	0x8000
#define APFS_BTNODE_MASK		0x0007	/* Valid on-disk flags */

/* B-tree location constants */
#define APFS_BTOFF_INVALID		0xffff

/*
 * Structure storing a location inside a B-tree node
 */
struct apfs_nloc {
	__le16 off;
	__le16 len;
} __packed;

/*
 * Structure storing the location of a key/value pair within a B-tree node
 */
struct apfs_kvloc {
	struct apfs_nloc k;
	struct apfs_nloc v;
} __packed;

/*
 * Structure storing the location of a key/value pair within a B-tree node
 * having fixed-size key and value (flag APFS_BTNODE_FIXED_KV_SIZE is present)
 */
struct apfs_kvoff {
	__le16 k;
	__le16 v;
} __packed;

/*
 * On-disk representation of a B-tree node
 */
struct apfs_btree_node_phys {
/*00*/	struct apfs_obj_phys btn_o;
/*20*/	__le16 btn_flags;
	__le16 btn_level;
	__le32 btn_nkeys;
/*28*/	struct apfs_nloc btn_table_space;
	struct apfs_nloc btn_free_space;
	struct apfs_nloc btn_key_free_list;
	struct apfs_nloc btn_val_free_list;
/*38*/	__le64 btn_data[];
} __packed;

/*
 * Structure used to store information about a B-tree that won't change
 * over time
 */
struct apfs_btree_info_fixed {
	__le32 bt_flags;
	__le32 bt_node_size;
	__le32 bt_key_size;
	__le32 bt_val_size;
} __packed;

/*
 * Structure used to store information about a B-tree (located at the end of
 * a B-tree root node block)
 */
struct apfs_btree_info {
	struct apfs_btree_info_fixed bt_fixed;
	__le32 bt_longest_key;			/* Longest key ever stored */
	__le32 bt_longest_val;			/* Longest value ever stored */
	__le64 bt_key_count;
	__le64 bt_node_count;
} __packed;

/*
 * In-memory representation of an APFS node
 */
struct node {
	u16 flags;		/* Node flags */
	u32 records;		/* Number of records in the node */
	int level;		/* Number of child levels below this node */

	int toc;		/* Offset of the TOC in the block */
	int key;		/* Offset of the key area in the block */
	int free;		/* Offset of the free area in the block */
	int data;		/* Offset of the data area in the block */

	u8 *free_key_bmap;	/* Free space bitmap for the key area */
	u8 *free_val_bmap;	/* Free space bitmap for the value area */
	u8 *used_key_bmap;	/* Used space bitmap for the key area */
	u8 *used_val_bmap;	/* Used space bitmap for the value area */

	struct btree *btree;			/* Btree the node belongs to */
	struct apfs_btree_node_phys *raw;	/* Raw node in memory */
	struct object object;			/* Object holding the node */
};

/**
 * apfs_node_is_leaf - Check if a b-tree node is a leaf
 * @node: the node to check
 */
static inline bool node_is_leaf(struct node *node)
{
	return (node->flags & APFS_BTNODE_LEAF) != 0;
}

/**
 * apfs_node_is_root - Check if a b-tree node is the root
 * @node: the node to check
 */
static inline bool node_is_root(struct node *node)
{
	return (node->flags & APFS_BTNODE_ROOT) != 0;
}

/**
 * apfs_node_has_fixed_kv_size - Check if a b-tree node has fixed key/value
 * sizes
 * @node: the node to check
 */
static inline bool node_has_fixed_kv_size(struct node *node)
{
	return (node->flags & APFS_BTNODE_FIXED_KV_SIZE) != 0;
}

/* Flags for the query structure */
#define QUERY_TREE_MASK		0007	/* Which b-tree we query */
#define QUERY_OMAP		0001	/* This is a b-tree object map query */
#define QUERY_CAT		0002	/* This is a catalog tree query */
#define QUERY_EXTENTREF		0004	/* This is an extentref tree query */
#define QUERY_MULTIPLE		0010	/* Search for multiple matches */
#define QUERY_NEXT		0020	/* Find next of multiple matches */
#define QUERY_EXACT		0040	/* Search for an exact match */
#define QUERY_DONE		0100	/* The search at this level is over */

/*
 * Structure used to retrieve data from an APFS B-Tree. For now only used
 * on the calalog and the object map.
 */
struct query {
	struct node *node;		/* Node being searched */
	struct key *key;		/* What the query is looking for */

	struct query *parent;		/* Query for parent node */
	unsigned int flags;

	/* Set by the query on success */
	int index;			/* Index of the entry in the node */
	int key_off;			/* Offset of the key in the node */
	int key_len;			/* Length of the key */
	int off;			/* Offset of the data in the node */
	int len;			/* Length of the data */

	int depth;			/* Put a limit on recursion */
};

/* In-memory tree types */
#define BTREE_TYPE_OMAP		1 /* The tree is an object map */
#define BTREE_TYPE_CATALOG	2 /* The tree is a catalog */
#define BTREE_TYPE_EXTENTREF	3 /* The tree is for extent references */
#define BTREE_TYPE_SNAP_META	4 /* The tree is for snapshot metadata */

/* In-memory structure representing a b-tree */
struct btree {
	u8 type;		/* Type of the tree */
	struct node *root;	/* Root of this b-tree */
	struct node *omap_root;	/* Root of its object map (can be NULL) */

	/* B-tree stats as measured by the fsck */
	u64 key_count;		/* Number of keys */
	u64 node_count;		/* Number of nodes */
	int longest_key;	/* Length of longest key */
	int longest_val;	/* Length of longest value */
};

/**
 * btree_is_omap - Check if a b-tree is an object map
 * @btree: the b-tree to check
 */
static inline bool btree_is_omap(struct btree *btree)
{
	return btree->type == BTREE_TYPE_OMAP;
}

/**
 * btree_is_snap_meta - Check if a b-tree is for snapshot metadata
 * @btree: the b-tree to check
 */
static inline bool btree_is_snap_meta(struct btree *btree)
{
	return btree->type == BTREE_TYPE_SNAP_META;
}

/**
 * btree_is_catalog - Check if a b-tree is a catalog
 * @btree: the b-tree to check
 */
static inline bool btree_is_catalog(struct btree *btree)
{
	return btree->type == BTREE_TYPE_CATALOG;
}

/**
 * btree_is_extentref - Check if a b-tree is for extent references
 * @btree: the b-tree to check
 */
static inline bool btree_is_extentref(struct btree *btree)
{
	return btree->type == BTREE_TYPE_EXTENTREF;
}

extern struct btree *parse_snap_meta_btree(u64 oid);
extern struct btree *parse_extentref_btree(u64 oid);
extern struct btree *parse_omap_btree(u64 oid);
extern struct btree *parse_cat_btree(u64 oid, struct node *omap_root);
extern struct query *alloc_query(struct node *node, struct query *parent);
extern void free_query(struct query *query);
extern int btree_query(struct query **query);
extern struct node *omap_read_node(u64 id);
extern void omap_lookup(struct node *tbl, u64 id, struct omap_record *omap_rec);
extern void extentref_lookup(struct node *tbl, u64 bno,
			     struct extref_record *extref);

#endif	/* _BTREE_H */
