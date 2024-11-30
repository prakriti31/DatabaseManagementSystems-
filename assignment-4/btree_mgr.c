#include "btree_mgr.h"
#include <math.h>
#include <ctype.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "dberror.h"
#include "tables.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void printTree(BTreeHandle *tree) {
    metaData *meta_data = (metaData *)tree->mgmtData;
    node *current_node = meta_data->root;

    if (current_node == NULL) {
        printf("Tree is empty\n");
        return;
    }

    // Use a queue to do a level-order traversal (breadth-first traversal)
    node **queue = (node **)malloc(sizeof(node*) * (meta_data->nodes));
    int front = 0, rear = 0;

    // Enqueue the root node
    queue[rear++] = current_node;

    // Traverse the tree level by level
    while (front < rear) {
        int current_level_size = rear - front;
        printf("Level %d: ", front);

        // Process each node in the current level
        for (int i = 0; i < current_level_size; i++) {
            node *current = queue[front++];

            // Print the keys in the current node
            printf("Node: ");
            for (int j = 0; j < current->num_keys; j++) {
                printf("%d ", current->keys[j].v.intV);
            }
            printf("| ");

            // Enqueue the child nodes (if any)
            if (!current->is_leaf) {
                for (int j = 0; j <= current->num_keys; j++) {
                    queue[rear++] = (node *)current->ptrs[j];
                }
            }
        }

        printf("\n");
    }

    free(queue);
}


RC initIndexManager(void *mgmtData) {
    printf("Index Manager was born\n");

    return RC_OK;
}

// Shutdown the B+ Tree index manager
RC shutdownIndexManager(void *mgmtData) {
    printf("Index Manager was killed\n");
    return RC_OK;
}

void * createNode(int n, bool is_leaf, bool is_root) {
    node * newNode = (node *) malloc(sizeof(node));
    newNode->keys = (Value *) malloc(sizeof(struct Value) * n); // Fixed DT_INT // [(int), (int), .... , n]
    newNode->ptrs = (void **) malloc(sizeof(void *) * (n + 1)); // No. of pointers will always be +1 than No. of Keys
    newNode->rids = (RID *) malloc(sizeof(struct RID) * (n + 1));
    newNode->num_keys = 0;
    newNode->is_leaf = is_leaf;
    newNode->next_leaf = NULL;
    newNode->parent = NULL;
    newNode->is_root = is_root;
    newNode->max_keys_per_node = n;
    newNode->max_ptrs_per_node = n + 1;
    return newNode;
}

RC createBtree(char *idxId, DataType keyType, int n) {
    // Initialize a new pagefile for holding the index
    createPageFile(idxId); // creates pagefile, writes /0' bytes, closes pagefile

    metaData *meta_data = (metaData *) malloc(sizeof(metaData));
    meta_data->order = n; // Setting the order
    meta_data->type = keyType; // Setting the keytype for the key
    meta_data->entries = 0; // Sum of all entries inside all the nodes

    node *root = createNode(meta_data->order,true, true); // This node is a root, which is by default a leaf as well.
    meta_data->root = root;
    meta_data->nodes = 1; // ROOT node

    // Need to write this data to the 1st (0th index) page of the file.
    // createPageFile already initialized the page by setting '/0' bytes
    SM_FileHandle file_handle;
    openPageFile(idxId, &file_handle);

    // How do we store it?
    // Serialize it, then deserialize it, making use of the struct
    // OR
    // Make use of offsets, fixed like PAGE_SIZE to logically partition page

    //Serialize -> Deserialize approach

    writeBlock(0,&file_handle,meta_data);

    // no need to check if the struct size is larger than PAGE_SIZE here,
    // as the test cases are guaranteed to be small

    // Data has been written to the first page now
    // Clean up

    free(meta_data);
    closePageFile(&file_handle);

    return RC_OK;

}

RC openBtree(BTreeHandle **tree, char *idxId) {

    SM_FileHandle file_handle;
    openPageFile(idxId, &file_handle);
    metaData *meta_data = (metaData *) malloc(sizeof(metaData));
    readBlock(0, &file_handle, meta_data);

    *tree = (BTreeHandle *) malloc(sizeof(BTreeHandle));
    (*tree)->idxId = idxId; // Storing filename in BTreeHandle
    (*tree)->mgmtData = meta_data;

    // Debugging code to check if data is correctly pointed by *tree
    // metaData *meta = (metaData *) (*tree)->mgmtData;
    // printf("Metadata: Order = %d, Entries = %d\n", meta->order, meta->entries);


    // printf("%d\n",btree->mgmtData);


    closePageFile(&file_handle);
    return RC_OK;
}

RC closeBtree(BTreeHandle *tree) {
    printf("We are closing Btree!!!\n");
    free(tree);
    return RC_OK;
}

// Delete a B+ Tree index
RC deleteBtree(char *idxId) {
    printf("We are deleting Btree!!!\n");
    if(remove(idxId)!=0)        //check whether the name exists
        return RC_FILE_NOT_FOUND;
    return RC_OK;
}

// Get the number of nodes in the B+ Tree
RC getNumNodes(BTreeHandle *tree, int *result) {
    metaData *meta_data = (metaData *)tree->mgmtData;
    printf("Current numNodes: %d\n", meta_data->nodes);  // Debugging output (only in DEBUG mode)

    *result = meta_data->nodes;
    return RC_OK;
}

// Get the number of entries in the B+ Tree
RC getNumEntries(BTreeHandle *tree, int *result) {
    metaData *meta_data = (metaData *)tree->mgmtData;
    printf("Current Entries: %d\n", meta_data->entries);  // Debugging output (only in DEBUG mode)

    *result = meta_data->entries;
    return RC_OK;
}


// Get the key type of the B+ Tree
RC getKeyType(BTreeHandle *tree, DataType *result) {
    *result = tree->keyType;

    return RC_OK;
}

int compareKeys(Value *key1, Value *key2) {
    if (key1->v.intV < key2->v.intV) return -1;
    if (key1->v.intV > key2->v.intV) return 1;
    return 0;
}

// Find a key in the B+ Tree
RC findKey(BTreeHandle *tree, Value *key, RID *result) {
    // First get the root, as it the starting point for our search
    metaData *meta_data = (metaData *)tree->mgmtData;
    node *current_node = meta_data->root;

    // 1. Traverse the tree to find the appropriate leaf node
    while (!current_node->is_leaf) {
        // Traverse internal nodes, find the correct child pointer to follow
        int i = 0;
        while (i < current_node->num_keys && compareKeys(key, &current_node->keys[i]) >= 0) { // compareKeys(k1,k2) - k1>k2 -> return 1 || k1 < k2 -> return -1
            i++;
        }
        current_node = (node *)current_node->ptrs[i];
    }
    for(int j = 0; j < current_node->num_keys; j++) {
        if(compareKeys(key, &current_node->keys[j]) == 0) {
            *result = current_node->rids[j];
            return RC_OK;
        }
    }
    return RC_IM_KEY_NOT_FOUND;
}


void sortKeys(Value *arr, void **ptr, int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (arr[j].v.intV > arr[j + 1].v.intV) {
                // Swap the keys
                int temp_key = arr[j].v.intV;
                arr[j].v.intV = arr[j + 1].v.intV;
                arr[j + 1].v.intV = temp_key;

                // Swap the pointers
                void *temp_ptr = ptr[j];
                ptr[j] = ptr[j + 1];
                ptr[j + 1] = temp_ptr;
            }
        }
    }
}

void insertIntoParent(node *parent, Value *key, RID rid) {
    node *new_node = parent;
    int numKeys = new_node->num_keys;
    new_node->keys[numKeys] = *key;
    new_node->ptrs[numKeys+1] = (void *)(&rid);
    new_node->rids[numKeys] = rid;
    sortKeys(new_node->keys, new_node->ptrs, numKeys);
    new_node->num_keys++;
}

// Insert key into the B+ Tree
RC insertKey(BTreeHandle *tree, Value *key, RID rid) {
    printf("Inserting Key: %d\n", key->v.intV);

    metaData *meta_data = (metaData *)tree->mgmtData;
    node *current_node = meta_data->root;

    current_node->parent = current_node;
    // 1. Traverse the tree to find the appropriate leaf node where the key should be inserted
    while (!current_node->is_leaf) {
        // Traverse internal nodes, find the correct child pointer to follow
        int i = 0;
        while (i < current_node->num_keys && compareKeys(key, &current_node->keys[i]) >= 0) { // compareKeys(k1,k2) - k1>k2 -> return 1 || k1 < k2 -> return -1
            i++;
        }
        current_node = (node *)current_node->ptrs[i];
    }

    // -------------------------------------------------------------------------------

    // 2. Inserting
    if (current_node->num_keys < current_node->max_keys_per_node) {
        // Insert the new key and the corresponding pointer (RID)
        int numKeys = current_node->num_keys;
        current_node->keys[numKeys] = *key;
        current_node->ptrs[numKeys] = (void *)(&rid);
        current_node->rids[numKeys] = (rid);
        if(numKeys + 1 > 1) { // Only sort if we have more than 1 keys
            sortKeys(current_node->keys, current_node->ptrs, numKeys);
        }
        current_node->num_keys++;
        meta_data->entries++;

        printTree(tree);
        return RC_OK;
    }

    // -------------------------------------------------------------------------------

    // 3. If node is overflowed, creating new node (Splitting)
    node *new_node = createNode(meta_data->order,true, false);
    meta_data->nodes++;
    new_node->parent = current_node->parent; // Fails if we have one leaf node and another root node // Fixed this by setting the parent of root as the parent itself, may cause recursion related errors??
    current_node->next_leaf = new_node;
    // ----------------------------------------------------------------
    // Add the new key in existing keys array, then sort, then split
    // meta_data->Entries++ karna hai
    // ----------------------------------------------------------------

    int mid = current_node->max_keys_per_node / 2;
    for (int i = mid + 1; i < current_node->num_keys; i++) {
        new_node->keys[i - (mid + 1)] = current_node->keys[i];
        new_node->ptrs[i - (mid + 1)] = current_node->ptrs[i];
        new_node->rids[i - (mid + 1)] = current_node->rids[i];
        new_node->num_keys++;
        meta_data->entries++;
    }

    if (compareKeys(key, &current_node->keys[mid]) >= 0) {
        // Insert new key after copying
        int insert_pos = new_node->num_keys;
        new_node->keys[insert_pos] = *key;
        new_node->ptrs[insert_pos] = (void *)(&rid);
        new_node->rids[insert_pos] = (rid);
        new_node->num_keys++;
        meta_data->entries++;
        sortKeys(new_node->keys, new_node->ptrs, new_node->num_keys);
    }

    // We are not deleting entries from current_node->keys, hence limiting num_keys
    current_node->num_keys = mid + 1;

    if (current_node->is_root == true) {
        node *new_root = createNode(meta_data->order,false, true);
        meta_data->nodes++;
        new_root->keys[0] = new_node->keys[0];
        new_root->num_keys++;
        new_root->ptrs[0] = current_node;
        new_root->ptrs[1] = new_node;

        current_node->parent = new_root;
        new_node->parent = new_root;
        current_node->is_root = false;

        meta_data->root = new_root;
    }
    // ----------------------------------------------------------------
    // Infinite loop ho raha hai because we are not handling non-leaf node. If current_node is roo
    // ----------------------------------------------------------------
    else {
        insertIntoParent(new_node->parent, key, rid);
        meta_data->root->ptrs[current_node->num_keys] = new_node;
    }

    printTree(tree);
    return RC_OK;
}

void deleteKeyValue(node *node, Value *key) {
    int key_value = key->v.intV;
    int size = node->num_keys;
    // Identify the element to be removed (1) and its index
    int indexToRemove = -1;
    for (int i = 0; i < size; i++) {
        if (node->keys[i].v.intV == key_value) {
            indexToRemove = i;
            break;
        }
    }

    // If the element was found, swap it with the last element
    if (indexToRemove != -1) {
        // Swap the keys (values)
        int temp_key = node->keys[indexToRemove].v.intV;
        node->keys[indexToRemove].v.intV = node->keys[size - 1].v.intV;
        node->keys[size - 1].v.intV = temp_key;

        // Swap the pointers
        void *temp_ptr = node->ptrs[indexToRemove];
        node->ptrs[indexToRemove] = node->ptrs[size - 1];
        node->ptrs[size - 1] = temp_ptr;

        // Swap the RIDs (if needed)
        RID temp_rid = node->rids[indexToRemove];
        node->rids[indexToRemove] = node->rids[size - 1];
        node->rids[size - 1] = temp_rid;

        // node->num_keys--;
    }
}

RC deleteKey(BTreeHandle *tree, Value *key) {
    printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\nDeleting Key: %d\n", key->v.intV);

    metaData *meta_data = (metaData *)tree->mgmtData;
    node *current_node = meta_data->root;

    // 1. Traverse the tree to find the appropriate leaf node where the key should be inserted
    while (!current_node->is_leaf) {
        // Traverse internal nodes, find the correct child pointer to follow
        int i = 0;
        while (i < current_node->num_keys && compareKeys(key, &current_node->keys[i]) >= 0) { // compareKeys(k1,k2) - k1>k2 -> return 1 || k1 < k2 -> return -1
            i++;
        }
        current_node = (node *)current_node->ptrs[i];
    }

    if (current_node->num_keys - 1 >= (int)(floor(current_node->max_keys_per_node+1)/2)) {
        printf("num_keys: %d\n", current_node->num_keys);
        deleteKeyValue(current_node, key);
        current_node->num_keys--;
        meta_data->entries--;
        sortKeys(current_node->keys, current_node->ptrs, current_node->num_keys);
    }
    else if (current_node->num_keys -1 < (int)(floor(current_node->max_keys_per_node+1)/2)) {
        printf("num_keys: %d\n", current_node->num_keys);
        deleteKeyValue(current_node, key);
        current_node->num_keys--;
        meta_data->entries--;
    }
    printf("num_keys: %d\n", current_node->num_keys);
    printTree(tree);

    return RC_OK;
 }

// Open a scan on the B+ Tree
RC openTreeScan(BTreeHandle *tree, BT_ScanHandle **handle) {
    metaData *meta_data = (metaData *)tree->mgmtData;
    node *current_node = meta_data->root;

    return RC_OK;
}

// Get the next entry in the scan
RC nextEntry(BT_ScanHandle *handle, RID *result) {
    return RC_OK;
}

// Close the scan on the B+ Tree
RC closeTreeScan(BT_ScanHandle *handle) {
    free(handle);
    return RC_OK;
}