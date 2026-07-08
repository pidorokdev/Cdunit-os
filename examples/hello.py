# Hello World in Python for Dunit OS
# Run with: run hello.py

def greet(name):
    return f"Hello, {name}!"

def main():
    print("=" * 40)
    print("  Welcome to Dunit OS Python Demo")
    print("=" * 40)
    
    message = greet("Dunit OS User")
    print(message)
    
    # Simple calculation demo
    numbers = [1, 2, 3, 4, 5]
    total = sum(numbers)
    print(f"Sum of {numbers} = {total}")
    
    print("\nPython is working on Dunit OS!")

if __name__ == "__main__":
    main()
