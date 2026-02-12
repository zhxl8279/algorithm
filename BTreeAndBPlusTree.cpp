#include <vector>
#include <deque>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <random>

template<typename KeyType>
class BTreeNode {
public:
    bool isLeaf;
    int nKeys;
    std::vector<KeyType> keys;
    std::vector<BTreeNode*> children;
    int t;  // 最小度数
    BTreeNode(int _t, bool _isLeaf)
        : isLeaf(_isLeaf), nKeys(0), t(_t) {
        // keys 最多 2t-1, children 最多 2t
        keys.reserve(2 * t - 1);
        children.reserve(2 * t);
    }
    // 查找 key 在节点中的索引或应插入位置
    int findKey(const KeyType& k) {
        int idx = 0;
        // 线性查找也可换二分：std::lower_bound
        while (idx < nKeys && keys[idx] < k)
            ++idx;
        return idx;
    }
    // 插入到非满节点
    void insertNonFull(const KeyType& k) {
        int i = nKeys - 1;
        if (isLeaf) {
            // 在叶子节点中插入：找到位置并插入
            keys.push_back(KeyType());  // 扩容占位
            while (i >= 0 && k < keys[i]) {
                keys[i + 1] = keys[i];
                --i;
            }
            keys[i + 1] = k;
            nKeys++;
        }
        else {
            // 内部节点：找到子节点 index
            int idx = findKey(k);
            // 若孩子已满，需要先分裂
            if (children[idx]->nKeys == 2 * t - 1) {
                splitChild(idx);
                // 分裂后看是否 k 应该落在右侧新节点
                if (keys[idx] < k)
                    idx++;
            }
            children[idx]->insertNonFull(k);
        }
    }

    // 分裂 children[idx]
    void splitChild(int idx) {
        BTreeNode* y = children[idx];
        BTreeNode* z = new BTreeNode(y->t, y->isLeaf);
        z->nKeys = t - 1;
        // 将 y.keys[t..2t-2] 移至 z
        for (int j = 0; j < t - 1; ++j)
            z->keys.push_back(y->keys[j + t]);
        // 如果 y 非叶子，移动子指针
        if (!y->isLeaf) {
            for (int j = 0; j < t; ++j)
                z->children.push_back(y->children[j + t]);
        }
        y->nKeys = t - 1;
        // 在 children 中插入 z
        children.insert(children.begin() + idx + 1, z);
        // 在 keys 中插入 y->keys[t-1]
        keys.insert(keys.begin() + idx, y->keys[t - 1]);
        nKeys++;
        // 清理 y 多余的 keys/children（可选，为简化例子不释放底层内存）
        y->keys.resize(t - 1);
        if (!y->isLeaf)
            y->children.resize(t);
    }
    // 遍历（调试用）
    void traverse(int depth = 0) {
        // 缩进
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "[";
        for (int i = 0; i < nKeys; ++i) {
            std::cout << keys[i];
            if (i + 1 < nKeys) std::cout << ", ";
        }
        std::cout << "]";
        std::cout << (isLeaf ? " (leaf)" : "") << "\n";
        if (!isLeaf) {
            for (int i = 0; i <= nKeys; ++i) {
                children[i]->traverse(depth + 1);
            }
        }
    }

    // 搜索 key
    BTreeNode* search(const KeyType& k) {
        int i = findKey(k);
        if (i < nKeys && keys[i] == k)
            return this;
        if (isLeaf)
            return nullptr;
        return children[i]->search(k);
    }

    // 删除 key 的公共接口
    void remove(const KeyType& k) {
        int idx = findKey(k);
        if (idx < nKeys && keys[idx] == k) {
            // key 在此节点
            if (isLeaf) {
                // 叶子节点直接删除
                keys.erase(keys.begin() + idx);
                nKeys--;
            }
            else {
                removeFromNonLeaf(idx);
            }
        }
        else {
            // key 不在此节点
            if (isLeaf) {
                // 不存在
                std::cout << "Key " << k << " does not exist in the tree\n";
                return;
            }
            // 决定进入子节点 children[idx]
            bool flag = (idx == nKeys);
            // 如果 children[idx] 关键字数不足，需要先填充
            if (children[idx]->nKeys < t) {
                fill(idx);
            }
            // 如果最初 idx==nKeys，并且 fill 导致合并后 children[idx-1]，则递归在 children[idx-1]
            if (flag && idx > nKeys)
                children[idx - 1]->remove(k);
            else
                children[idx]->remove(k);
        }
    }

    // 从非叶节点删除 keys[idx]
    void removeFromNonLeaf(int idx) {
        KeyType k = keys[idx];
        // 前驱子树 children[idx]
        if (children[idx]->nKeys >= t) {
            KeyType pred = getPredecessor(idx);
            keys[idx] = pred;
            children[idx]->remove(pred);
        }
        // 后继子树 children[idx+1]
        else if (children[idx + 1]->nKeys >= t) {
            KeyType succ = getSuccessor(idx);
            keys[idx] = succ;
            children[idx + 1]->remove(succ);
        }
        else {
            // 两侧子节点都只有 t-1 个 key，合并 idx 和 idx+1
            merge(idx);
            children[idx]->remove(k);
        }
    }

    KeyType getPredecessor(int idx) {
        BTreeNode* cur = children[idx];
        while (!cur->isLeaf)
            cur = cur->children[cur->nKeys];
        return cur->keys[cur->nKeys - 1];
    }

    KeyType getSuccessor(int idx) {
        BTreeNode* cur = children[idx + 1];
        while (!cur->isLeaf)
            cur = cur->children[0];
        return cur->keys[0];
    }

    // fill children[idx] 使其至少有 t 个关键字
    void fill(int idx) {
        // 如果前兄弟有多余，借
        if (idx > 0 && children[idx - 1]->nKeys >= t) {
            borrowFromPrev(idx);
        }
        // 后兄弟有多余，借
        else if (idx < nKeys && children[idx + 1]->nKeys >= t) {
            borrowFromNext(idx);
        }
        else {
            // 合并
            if (idx < nKeys)
                merge(idx);
            else
                merge(idx - 1);
        }
    }

    void borrowFromPrev(int idx) {
        BTreeNode* child = children[idx];
        BTreeNode* sibling = children[idx - 1];
        // child 的 keys 后移，腾出位置
        child->keys.insert(child->keys.begin(), keys[idx - 1]);
        if (!child->isLeaf) {
            child->children.insert(child->children.begin(), sibling->children.back());
            sibling->children.pop_back();
        }
        // 把 sibling 最后一个 key 上移到父节点
        keys[idx - 1] = sibling->keys.back();
        sibling->keys.pop_back();
        sibling->nKeys--;
        child->nKeys++;
    }

    void borrowFromNext(int idx) {
        BTreeNode* child = children[idx];
        BTreeNode* sibling = children[idx + 1];
        // 把父节点 keys[idx] 下移到 child
        child->keys.push_back(keys[idx]);
        if (!child->isLeaf) {
            child->children.push_back(sibling->children.front());
            sibling->children.erase(sibling->children.begin());
        }
        // sibling 第一个 key 上移到父节点
        keys[idx] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());
        sibling->nKeys--;
        child->nKeys++;
    }

    // 合并 children[idx] 和 children[idx+1]
    void merge(int idx) {
        BTreeNode* child = children[idx];
        BTreeNode* sibling = children[idx + 1];
        // 把父节点 keys[idx] 下移到 child
        child->keys.push_back(keys[idx]);
        // 将 sibling 的 keys 和 children 追加到 child
        for (int i = 0; i < sibling->nKeys; ++i)
            child->keys.push_back(sibling->keys[i]);
        if (!child->isLeaf) {
            for (int i = 0; i <= sibling->nKeys; ++i)
                child->children.push_back(sibling->children[i]);
        }
        // 更新 child 关键字数
        child->nKeys += sibling->nKeys + 1;
        // 从父节点移除 keys[idx] 和 children[idx+1]
        keys.erase(keys.begin() + idx);
        children.erase(children.begin() + idx + 1);
        nKeys--;
        // 释放 sibling（可选）
        delete sibling;
    }
};

template<typename KeyType>
class BTree {
public:
    BTree(int _t) : root(nullptr), t(_t) {}

    // 遍历
    void traverse() {
        if (root) root->traverse();
        else std::cout << "Empty tree\n";
    }

    // 搜索
    BTreeNode<KeyType>* search(const KeyType& k) {
        return root ? root->search(k) : nullptr;
    }

    // 插入
    void insert(const KeyType& k) {
        if (!root) {
            root = new BTreeNode<KeyType>(t, true);
            root->keys.push_back(k);
            root->nKeys = 1;
        }
        else {
            if (root->nKeys == 2 * t - 1) {
                BTreeNode<KeyType>* s = new BTreeNode<KeyType>(t, false);
                s->children.push_back(root);
                root = s;
                s->splitChild(0);
                s->insertNonFull(k);
            }
            else {
                root->insertNonFull(k);
            }
        }
    }

    // 删除
    void remove(const KeyType& k) {
        if (!root) {
            std::cout << "Empty tree\n";
            return;
        }
        root->remove(k);
        if (root->nKeys == 0) {
            BTreeNode<KeyType>* tmp = root;
            if (root->isLeaf) {
                delete root;
                root = nullptr;
            }
            else {
                root = root->children[0];
                delete tmp;
            }
        }
    }
private:
    BTreeNode<KeyType>* root;
    int t;
};


// 简化：这里我们假设叶子节点存储 key（如有 value，可改为 pair）
template<typename KeyType>
class BPlusNode {
public:
    size_t t; // 最小度数或阶，根据需要定义；在内部节点最多 2t 子指针，叶子最多 2t keys
    bool isLeaf;
    std::deque<KeyType> keys;
    BPlusNode* parent;
    BPlusNode(size_t m, bool leaf) : t(m), isLeaf(leaf), parent(nullptr) {}
    virtual ~BPlusNode() = default;
    virtual void insert(KeyType) = 0;
    virtual bool remove(const KeyType&) = 0;
    virtual void traverse(int depth = 0) = 0;

    // 查找 key 在节点中的索引或应插入位置
    int findKey(const KeyType& k) {
        int idx = 0;
        // 线性查找也可换二分：std::lower_bound
        while (idx < keys.size() && keys[idx] < k)
            ++idx;
        return idx;
    }
};

template<typename KeyType>
class BPlusLeafNode : public BPlusNode<KeyType> {
public:
    using BPlusNode<KeyType>::isLeaf;
    using BPlusNode<KeyType>::keys;
    using BPlusNode<KeyType>::t;

    BPlusLeafNode* next;  // 叶子链表指针
    BPlusLeafNode* prev;
    // 存储 keys; 若需要存储 value，可改为 vector<pair<KeyType, ValueType>>
    std::deque<KeyType> values;
    BPlusLeafNode(size_t t) : BPlusNode<KeyType>(t, true), next(nullptr), prev(nullptr) {}

    void insert(KeyType k) override {
        auto it = std::lower_bound(keys.begin(), keys.end(), k);
        keys.insert(it, k);
    }

    bool remove(const KeyType& k) override {
        size_t idx = this->findKey(k);
        if (idx < keys.size() && keys[idx] == k) {
            // key 在此节点, 叶子节点直接删除
            keys.erase(keys.begin() + idx);
            return true;
        }
        return false;
    }

    // 遍历（调试用）
    void traverse(int depth = 0) {
        // 缩进
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "[";
        for (int i = 0; i < keys.size(); ++i) {
            std::cout << keys[i];
            if (i + 1 < keys.size()) std::cout << ", ";
        }
        std::cout << "]";
        std::cout << (isLeaf ? " (leaf)" : "") << "\n";
    }
};

template<typename KeyType>
class BPlusInternalNode : public BPlusNode<KeyType> {
public:
    using BPlusNode<KeyType>::isLeaf;
    using BPlusNode<KeyType>::keys;
    using BPlusNode<KeyType>::t;

    std::deque<BPlusNode<KeyType>*> children;
    BPlusInternalNode(size_t t) : BPlusNode<KeyType>(t, false) {}

    void insert(KeyType k) override {
        size_t idx = this->findKey(k);
        bool insertMax = (idx == keys.size());
        if (insertMax) --idx;
        children[idx]->insert(k);
        if (insertMax) keys.at(idx) = k;
        if (children[idx]->keys.size() > 2 * t) {
            splitChild(idx);
        }
    }

    void splitChild(size_t idx) {
        BPlusNode<KeyType>* left = children[idx];
        int num = left->keys.size();
        int midIndex = num / 2;

        // 创建新内部节点
        BPlusNode<KeyType>* right = nullptr;
        if (left->isLeaf) {
            BPlusLeafNode<KeyType> *newNode = new BPlusLeafNode<KeyType>(t);
            right = newNode;
            BPlusLeafNode<KeyType>* leftNode = static_cast<BPlusLeafNode<KeyType>*>(left);
            // 插入到链表
            newNode->next = leftNode->next;
            if (leftNode->next) leftNode->next->prev = newNode;
            leftNode->next = newNode;
            newNode->prev = leftNode;
        }
        else {
            BPlusInternalNode<KeyType> *newNode = new BPlusInternalNode<KeyType>(t);
            right = newNode;
            // 右半部分 children 和 keys 移动到 right
            BPlusInternalNode<KeyType>* leftNode = static_cast<BPlusInternalNode<KeyType>*>(left);
            // children 从 midIndex 开始移
            newNode->children.assign(leftNode->children.begin() + midIndex, leftNode->children.end());
            // 更新 parent 指针
            for (auto child : newNode->children) {
                child->parent = newNode;
            }
            leftNode->children.resize(midIndex);
        }

        // keys 从 midIndex 开始移（keys 数 = children.size()-1）
        right->keys.assign(left->keys.begin() + midIndex, left->keys.end());

        // 剪裁原 left
        left->keys.resize(midIndex);
        // 将 upKey 插入到父节点
        right->parent = left->parent;
        keys[idx] = left->keys.back();
        keys.insert(keys.begin() + idx + 1, right->keys.back());
        children.insert(children.begin() + idx + 1, right);
    }

    // 删除 key 的公共接口
    bool remove(const KeyType& k) override {
        size_t idx = this->findKey(k);
        if (idx == keys.size()) return false;
        if (!children[idx]->remove(k)) return false;
        keys.at(idx) = children[idx]->keys.back();
        if (children[idx]->keys.size() < t) {
            fill(idx);
        }
        return true;
    }

    // fill children[idx] 使其至少有 t 个关键字
    void fill(size_t idx) {
        BPlusNode<KeyType>* cur = children[idx];
        // 如果前兄弟有多余，借
        if (idx > 0 && children[idx-1]->keys.size() > t) {
            borrowFromPrev(idx);
        }
        // 后兄弟有多余，借
        else if ((idx + 1) < keys.size() && children[idx + 1]->keys.size() > t) {
            borrowFromNext(idx);
        }
        else {
            // 合并
            if (idx > 0) {
                mergePrev(idx);
            }
            else {
                mergeNext(idx);
            }
        }
    }

    void borrowFromPrev(size_t idx) {
        BPlusNode<KeyType>* child = children[idx];
        BPlusNode<KeyType>* sibling = children[idx - 1];
        // child 的 keys 后移，腾出位置
        child->keys.insert(child->keys.begin(), sibling->keys.back());
        sibling->keys.pop_back();
        if (!child->isLeaf) {
            BPlusInternalNode<KeyType>* childNode = static_cast<BPlusInternalNode<KeyType>*>(child);
            BPlusInternalNode<KeyType>* siblingNode = static_cast<BPlusInternalNode<KeyType>*>(sibling);
            childNode->children.insert(childNode->children.begin(), siblingNode->children.back());
            siblingNode->children.pop_back();
        }
        sibling->parent = child->parent;
        // 把 sibling 最后一个 key 上移到父节点
        keys[idx - 1] = sibling->keys.back();
    }

    void borrowFromNext(size_t idx) {
        BPlusNode<KeyType>* child = children[idx];
        BPlusNode<KeyType>* sibling = children[idx + 1];
        // child 的 keys 后移，腾出位置
        child->keys.insert(child->keys.end(), sibling->keys.front());
        sibling->keys.pop_front();
        if (!child->isLeaf) {
            BPlusInternalNode<KeyType>* childNode = static_cast<BPlusInternalNode<KeyType>*>(child);
            BPlusInternalNode<KeyType>* siblingNode = static_cast<BPlusInternalNode<KeyType>*>(sibling);
            childNode->children.insert(childNode->children.end(), siblingNode->children.front());
            siblingNode->children.pop_front();
        }
        sibling->parent = child->parent;
        // 把 sibling 最后一个 key 上移到父节点
        keys[idx + 1] = sibling->keys.front();
    }

    // 合并 children[idx] 和 children[idx-1]
    void mergePrev(size_t idx) {
        BPlusNode<KeyType>* child = children[idx];
        BPlusNode<KeyType>* sibling = children[idx - 1];
        // child 的 keys 后移，腾出位置
        child->keys.insert(child->keys.begin(), sibling->keys.begin(), sibling->keys.end());
        sibling->keys.clear();
        if (!child->isLeaf) {
            BPlusInternalNode<KeyType>* childNode = static_cast<BPlusInternalNode<KeyType>*>(child);
            BPlusInternalNode<KeyType>* siblingNode = static_cast<BPlusInternalNode<KeyType>*>(sibling);
            childNode->children.insert(childNode->children.begin(), siblingNode->children.begin(), siblingNode->children.end());
            siblingNode->children.clear();
        }
        // 把 sibling 最后一个 key 上移到父节点
        keys.erase(keys.begin() + idx - 1);
        children.erase(children.begin() + idx - 1);

        if (child->isLeaf) {
            BPlusLeafNode<KeyType>* childNode = static_cast<BPlusLeafNode<KeyType>*>(child);
            BPlusLeafNode<KeyType>* siblingNode = static_cast<BPlusLeafNode<KeyType>*>(sibling);
            childNode->prev = siblingNode->prev;
            if (siblingNode->prev) siblingNode->prev->next = childNode;
        }
        
        // 释放 sibling（可选）
        delete sibling;
    }

    // 合并 children[idx] 和 children[idx+1]
    void mergeNext(size_t idx) {
        BPlusNode<KeyType>* child = children[idx];
        BPlusNode<KeyType>* sibling = children[idx + 1];
        // child 的 keys 后移，腾出位置
        child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
        sibling->keys.clear();
        if (!child->isLeaf) {
            BPlusInternalNode<KeyType>* childNode = static_cast<BPlusInternalNode<KeyType>*>(child);
            BPlusInternalNode<KeyType>* siblingNode = static_cast<BPlusInternalNode<KeyType>*>(sibling);
            childNode->children.insert(childNode->children.end(), siblingNode->children.begin(), siblingNode->children.end());
            siblingNode->children.clear();
        }

        // 把 sibling 最后一个 key 上移到父节点
        keys.erase(keys.begin() + idx + 1);
        children.erase(children.begin() + idx + 1);
        keys[idx] = child->keys.back();

        if (child->isLeaf) {
            BPlusLeafNode<KeyType>* childNode = static_cast<BPlusLeafNode<KeyType>*>(child);
            BPlusLeafNode<KeyType>* siblingNode = static_cast<BPlusLeafNode<KeyType>*>(sibling);
            childNode->next = siblingNode->next;
            if (siblingNode->next) siblingNode->next->prev = childNode;
        }

        // 释放 sibling（可选）
        delete sibling;
    }

    // 遍历（调试用）
    void traverse(int depth = 0) {
        // 缩进
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "[";
        for (int i = 0; i < keys.size(); ++i) {
            std::cout << keys[i];
            if (i + 1 < keys.size()) std::cout << ", ";
        }
        std::cout << "]";
        std::cout << (isLeaf ? " (leaf)" : "") << "\n";
        if (!isLeaf) {
            for (int i = 0; i < children.size(); ++i) {
                children[i]->traverse(depth + 1);
            }
        }
    }
};

template<typename KeyType>
class BPlusTree {
public:
    size_t t;
    BPlusNode<KeyType>* root;
    BPlusTree(int _t) : root(nullptr), t(_t) {}

    // 搜索
    BPlusLeafNode<KeyType>* search(const KeyType& k) {
        if (!root) return nullptr;
        BPlusNode<KeyType>* cur = root;
        // 向下查找到叶子
        while (!cur->isLeaf) {
            auto inode = static_cast<BPlusInternalNode<KeyType>*>(cur);
            int idx = inode->findKey(k);
            cur = inode->children[idx];
        }
        auto leaf = static_cast<BPlusLeafNode<KeyType>*>(cur);
        // 在 leaf->keys 中查找
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), k);
        if (it != leaf->keys.end() && *it == k)
            return leaf;
        return nullptr;
    }

    // 插入
    void insert(const KeyType& k) {
        if (!root) {
            auto leaf = new BPlusLeafNode<KeyType>(t);
            leaf->keys.push_back(k);
            root = leaf;
            return;
        }
        root->insert(k);
        if (root->keys.size() > 2 * t) {
            auto s = new BPlusInternalNode<KeyType>(t);
            s->keys.push_back(root->keys.back());
            s->children.push_back(root);
            root->parent = s;
            root = s;
            s->splitChild(0);
        }
    }

    // 删除
    void remove(const KeyType& k) {
        if (!root) {
            std::cout << "Empty tree\n";
            return;
        }

        if (!root->remove(k)) {
            // 不存在
            std::cout << "Key " << k << " does not exist in the tree\n";
            return;
        }
        if (root->isLeaf) {
            if (root->keys.size() == 0) {
                delete root;
                root = nullptr;
            }
        }
        else {
            if (root->keys.size() == 1) {
                BPlusInternalNode<KeyType>* tmp = static_cast<BPlusInternalNode<KeyType>*>(root);
                root = tmp->children[0];
                delete tmp;
            }
        }
    }

    // 遍历叶子链表，调试用
    void traverseLeaves() {
        // 找到最左叶
        BPlusNode<KeyType>* cur = root;
        if (!cur) {
            std::cout << "Empty B+ tree\n";
            return;
        }
        while (!cur->isLeaf) {
            cur = static_cast<BPlusInternalNode<KeyType>*>(cur)->children[0];
        }
        BPlusLeafNode<KeyType>* leaf = static_cast<BPlusLeafNode<KeyType>*>(cur);
        // 顺序打印所有叶
        while (leaf) {
            std::cout << "[";
            for (auto& k : leaf->keys) std::cout << k << " ";
            std::cout << "] -> ";
            leaf = leaf->next;
        }
        std::cout << "NULL\n";
    }

    // 遍历
    void traverse() {
        if (root) root->traverse();
        else std::cout << "Empty tree\n";
    }
};

int main() {
    {
        int t = 10; // 最小度数
        std::vector<int> keysToInsert(100);
        std::iota(keysToInsert.begin(), keysToInsert.end(), 1);

        BTree<int> tree(t);
        for (int k : keysToInsert) {
            std::cout << "Insert " << k << ":\n";
            tree.insert(k);
            tree.traverse();
            std::cout << "-------------------\n";
        }
        int searchKey = 6;
        auto node = tree.search(searchKey);
        if (node)
            std::cout << "Found key " << searchKey << " in a node.\n";
        else
            std::cout << "Key " << searchKey << " not found.\n";
        // 删除示例
        tree.remove(6);
        std::cout << "After removing 6:\n";
        tree.traverse();
    }

    {
        int t = 4; // 最小度数
        std::vector<int> keysToInsert(100);
        std::iota(keysToInsert.begin(), keysToInsert.end(), 1);
        //std::random_shuffle(keysToInsert.begin(), keysToInsert.end());
        BPlusTree<int> tree(t);
        for (int k : keysToInsert) {
            std::cout << "Insert " << k << ":\n";
            tree.insert(k);
            tree.traverse();
            std::cout << "-------------------\n";
        }
        int searchKey = 6;
        auto node = tree.search(searchKey);
        if (node)
            std::cout << "Found key " << searchKey << " in a node.\n";
        else
            std::cout << "Key " << searchKey << " not found.\n";
        // 删除示例
        tree.remove(searchKey);
        std::cout << "After removing 6:\n";
        tree.traverse();
        tree.traverseLeaves();

        std::cout << "-------------------\n";
        for (int i = 1; i <= keysToInsert.size(); ++i) {
            //std::cout << "\n\n-------------------";
            int searchKey = i;
            auto node = tree.search(searchKey);
            if (node) {
                std::cout << "Found key " << searchKey << " in a node.\n";
            }
            else {
                std::cout << "Key " << searchKey << " not found.\n";
            }
            // 删除示例
            tree.remove(searchKey);
            std::cout << "After removing " << searchKey << ":\n";
            tree.traverse();
            std::cout << "---------\n";
            tree.traverseLeaves();
        }
        tree.traverse();
        std::cout << "---------\n";
        tree.traverseLeaves();
    }
    return 0;
}
