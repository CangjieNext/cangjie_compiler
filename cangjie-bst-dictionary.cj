enum BSTNode<K, V> where K: Comparable {
    case Empty
    case Node(K, V, BSTNode<K, V>, BSTNode<K, V>)
}

struct BST<K, V> where K: Comparable {
    var root: BSTNode<K, V>

    func insert(_ tree: BSTNode<K, V>, _ key: K, _ value: V): BSTNode<K, V> {
        match (tree) {
            case Empty => Node(key, value, Empty, Empty)
            case Node(k, v, left, right) =>
                if (key < k) {
                    Node(k, v, insert(left, key, value), right)
                } else if (key > k) {
                    Node(k, v, left, insert(right, key, value))
                } else {
                    Node(k, value, left, right)
                }
        }
    }

    func find(_ tree: BSTNode<K, V>, _ key: K): Option<V> {
        match (tree) {
            case Empty => None
            case Node(k, v, left, right) =>
                if (key == k) {
                    Some(v)
                } else if (key < k) {
                    find(left, key)
                } else {
                    find(right, key)
                }
        }
    }

    func inorderTraversal(_ action: func(K, V): Unit): Unit {
        var stack = Array<BSTNode<K, V>>()
        var current = root
        while (true) {
            match (current) {
                case Node(k, v, left, right) =>
                    stack.push(current)
                    current = left
                case Empty =>
                    if (stack.isEmpty()) {
                        break
                    }
                    let top = stack.pop()!
                    match (top) {
                        case Node(k, v, _, right) =>
                            action(k, v)
                            current = right
                        case Empty => 
                    }
            }
        }
    }
}

main(): Unit {
    var bst = BST<String, String>(root: Empty)
    
    while (true) {
        println("\n========== 二叉搜索树存储器 ==========")
        println("1. 插入键值对")
        println("2. 查找键")
        println("3. 显示所有键值对（中序遍历）")
        println("4. 退出")
        print("请选择操作: ")
        let choice = readln()
        
        if (choice == "1") {
            print("请输入键（英文单词）: ")
            let key = readln()
            print("请输入值（中文释义）: ")
            let value = readln()
            bst.root = bst.insert(bst.root, key, value)
            println("插入/更新成功！")
            
        } else if (choice == "2") {
            print("请输入要查找的键: ")
            let key = readln()
            match (bst.find(bst.root, key)) {
                case Some(v) => println("${key}: ${v}")
                case None => println("未找到该键")
            }
            
        } else if (choice == "3") {
            println("--- 所有键值对（中序） ---")
            if (bst.root is Empty) {
                println("（树为空）")
            } else {
                bst.inorderTraversal() { k, v =>
                    println("${k}: ${v}")
                }
            }
            
        } else if (choice == "4") {
            println("再见！")
            break
            
        } else {
            println("无效输入，请重试")
        }
    }
}