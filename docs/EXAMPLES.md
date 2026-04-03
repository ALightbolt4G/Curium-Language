# 🚀 Curium Implementation Examples

This document showcases practical examples of common patterns and advanced features in the Curium language.

---

## 1. Linked List with Safe Pointers

This example demonstrates how to use the `^` safe pointer operator to build a data structure with explicit memory safety.

```cm
struct Node {
    val: int;
    next: ^Node; // Safe pointer to another Node
}

fn main() {
    // Create the first node
    mut n1: Node;
    n1.val = 10;
    
    // Create the second node
    mut n2: Node;
    n2.val = 20;
    
    // Link them together safely
    n1.next = ^n2; // Take address of n2 and store it in n1.next
    
    // Print the linked values
    print("Node 1: ");
    println(n1.val);
    
    if (n1.next != 0) {
        print("Node 2 (linked): ");
        println(n1.next.val); // Access linked node value
    }
}
```

---

## 2. Dynamic Calculator (Dynamic Operators)

Dynamic operators (`dyn`) allow you to change how an expression is evaluated at runtime based on string identifiers.

```cm
fn main() {
    mut x: float = 10.0;
    mut y: float = 5.0;
    mut action: string = "avg";

    // Define the 'action' dynamic operator
    dyn action in (
        "+" => { return x + y; },
        "*" => { return x * y; },
        "avg" => { return (x + y) / 2.0; }
    ) dyn($) {
        println("Error: Unknown operator!");
        return 0;
    };

    // Use the dynamic operator infix
    let result = x action y;
    
    print("Action '");
    print(action);
    print("' result: ");
    println(result);
}
```

---

## 3. High-Performance Matrix with Reactors

Reactors allow you to switch to a high-performance **Arena Allocation** model for complex computations, ensuring minimal latency.

```cm
fn main() {
    // Use an Arena reactor for 1024-byte pool
    reactor arena(1024) {
        println("Matrix computation started...");
        
        // Inside this block, every allocation for strings or pointers
        // happens within the arena for maximum speed.
        
        c {
            int matrix[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
            printf("Matrix trace: %d\n", matrix[0][0] + matrix[1][1] + matrix[2][2]);
        }
        
        println("Computation finished.");
    }
    // All memory used inside the reactor is reclaimed instantly here.
}
```

---

## 4. Error Handling (Option & Result)

Curium uses the `?` and `??` operators for safe error propagation, influenced by modern safety-first languages.

```cm
fn divide(a: int, b: int) -> Result<int, string> {
    if (b == 0) {
        return Err("Division by zero!");
    }
    return Ok(a / b);
}

fn main() {
    let res = divide(10, 2);
    
    // Use ? to propagate error or value
    let val: int = res?; 
    println("Result: " + val.to_str());
    
    // Use ?? for default values
    let opt: ?int = Some(42);
    let final_val = opt ?? 0;
    println("Final: " + final_val.to_str());
}
```
