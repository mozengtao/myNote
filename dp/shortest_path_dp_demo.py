#!/usr/bin/env python3
"""
Dynamic Programming Shortest Path Demo
=====================================

Demonstrates DP concepts through network shortest path algorithms:
- Bellman-Ford (Eventually Consistent DP)
- Dijkstra (Optimal Scheduling DP)

Run: python3 shortest_path_dp_demo.py
"""

from collections import defaultdict
import heapq
from typing import Dict, List, Tuple, Optional

class NetworkGraph:
    """Network topology representation with engineering insights"""
    
    def __init__(self):
        self.adj_list: Dict[str, List[Tuple[str, int]]] = defaultdict(list)
        self.nodes = set()
    
    def add_edge(self, u: str, v: str, weight: int):
        """Add directed edge from u to v with given weight"""
        self.adj_list[u].append((v, weight))
        self.nodes.add(u)
        self.nodes.add(v)
    
    def get_neighbors(self, node: str) -> List[Tuple[str, int]]:
        """Get all neighbors of a node"""
        return self.adj_list[node]
    
    def display(self):
        """Display network topology"""
        print("🌐 Network Topology:")
        for node in sorted(self.nodes):
            neighbors = self.adj_list[node]
            if neighbors:
                neighbor_str = ', '.join([f"{v}(cost={w})" for v, w in neighbors])
                print(f"  {node} → {neighbor_str}")
            else:
                print(f"  {node} → (no outgoing connections)")

def bellman_ford_shortest_path(graph: NetworkGraph, source: str) -> Dict[str, Optional[int]]:
    """
    Bellman-Ford algorithm - DP approach with convergence visualization
    
    Engineering Analogy: Distributed consensus algorithm
    - Each round propagates distance updates across network
    - Continues until no more improvements possible
    - Eventually consistent convergence model
    """
    
    # Initialize DP table (routing table initialization)
    INF = float('inf')
    dp = {node: INF for node in graph.nodes}
    dp[source] = 0
    
    print(f"\n🔄 Bellman-Ford Algorithm (source: {source})")
    print("=" * 55)
    print("💡 Engineering Insight: Eventually Consistent Convergence")
    print("   Like distributed consensus - keep propagating until stable")
    
    # Display initial state
    print(f"\nRound 0 (Initial): {dict(dp)}")
    
    num_nodes = len(graph.nodes)
    
    # Relaxation rounds (at most V-1 rounds needed)
    for round_num in range(1, num_nodes):
        updated = False
        old_dp = dp.copy()
        
        print(f"\n--- Round {round_num} Processing ---")
        
        # Process all edges (order doesn't matter - eventually consistent)
        for u in graph.nodes:
            for v, weight in graph.get_neighbors(u):
                # Core DP transition equation
                if dp[u] != INF and dp[u] + weight < dp[v]:
                    old_val = dp[v]
                    dp[v] = dp[u] + weight
                    updated = True
                    print(f"  ✅ Update: {u}→{v}: dp[{v}] = min({old_val}, {dp[u]}+{weight}) = {dp[v]}")
        
        # Show round results
        changes = []
        for node in graph.nodes:
            if old_dp[node] != dp[node]:
                changes.append(f"{node}: {old_dp[node]}→{dp[node]}")
        
        print(f"\nRound {round_num} Result: {dict(dp)}")
        if changes:
            print(f"Changes: {', '.join(changes)}")
        else:
            print("No changes - algorithm converged!")
        
        # Early termination if converged
        if not updated:
            print(f"\n✅ Converged after {round_num} rounds")
            print("🎯 Key Insight: DAG structure guarantees finite convergence")
            break
    
    # Convert INF back to None for unreachable nodes
    result = {node: (dist if dist != INF else None) for node, dist in dp.items()}
    return result

def dijkstra_shortest_path(graph: NetworkGraph, source: str) -> Dict[str, Optional[int]]:
    """
    Dijkstra's algorithm - Priority-driven DP scheduling
    
    Engineering Analogy: CPU scheduler with shortest job first
    - Always process node with minimum tentative distance
    - Optimal execution order guarantees single-pass correctness
    - Like event-driven architecture with priority queues
    """
    
    # Initialize DP table and priority queue  
    INF = float('inf')
    dp = {node: INF for node in graph.nodes}
    dp[source] = 0
    
    # Priority queue: (distance, node) - simulates ready queue
    pq = [(0, source)]
    visited = set()  # Finalized nodes (like completed tasks)
    
    print(f"\n🎯 Dijkstra's Algorithm (source: {source})")  
    print("=" * 55)
    print("💡 Engineering Insight: Optimal Priority-Driven Scheduling")
    print("   Like CPU scheduler - always process shortest job first")
    
    print(f"\nInitial state: {dict(dp)}")
    print("Priority Queue: [(distance, node)]")
    
    step = 1
    
    while pq:
        current_dist, u = heapq.heappop(pq)
        
        # Skip if already processed (duplicate entries in queue)
        if u in visited:
            continue
        
        # Mark as visited (finalized - like task completion)
        visited.add(u)
        
        print(f"\n--- Step {step}: Processing '{u}' (distance: {current_dist}) ---")
        print(f"🔄 Analogy: CPU executing task '{u}' with priority {current_dist}")
        
        # Relax all neighbors (schedule dependent tasks)
        updates = []
        new_tasks = []
        
        for v, weight in graph.get_neighbors(u):
            if v not in visited:  # Only update unfinalized nodes
                new_dist = current_dist + weight
                if new_dist < dp[v]:
                    old_dist = dp[v] 
                    dp[v] = new_dist
                    heapq.heappush(pq, (new_dist, v))
                    updates.append(f"{v}: {old_dist}→{new_dist}")
                    new_tasks.append(f"({new_dist}, {v})")
        
        if updates:
            print(f"  ✅ DP Updates: {', '.join(updates)}")
            print(f"  📋 New tasks scheduled: {', '.join(new_tasks)}")
        else:
            print(f"  ℹ️  No updates needed (all neighbors processed)")
        
        print(f"  📊 Current DP state: {dict(dp)}")
        print(f"  ✔️  Finalized nodes: {sorted(visited)}")
        
        step += 1
    
    # Convert INF back to None for unreachable nodes  
    result = {node: (dist if dist != INF else None) for node, dist in dp.items()}
    return result

def create_test_network() -> NetworkGraph:
    """Create the test network topology"""
    graph = NetworkGraph()
    
    # Network: A→B(1), A→C(4), B→D(2), C→D(1)
    graph.add_edge('A', 'B', 1)
    graph.add_edge('A', 'C', 4) 
    graph.add_edge('B', 'D', 2)
    graph.add_edge('C', 'D', 1)
    
    return graph

def create_complex_network() -> NetworkGraph:
    """Create a more complex network for demonstration"""
    graph = NetworkGraph()
    
    # More complex topology
    graph.add_edge('S', 'A', 2)
    graph.add_edge('S', 'B', 6)
    graph.add_edge('A', 'B', 3)
    graph.add_edge('A', 'C', 1) 
    graph.add_edge('B', 'C', 1)
    graph.add_edge('B', 'D', 2)
    graph.add_edge('C', 'D', 4)
    graph.add_edge('C', 'T', 3)
    graph.add_edge('D', 'T', 1)
    
    return graph

def analyze_dp_characteristics(graph: NetworkGraph):
    """Analyze DP characteristics of the problem"""
    print("\n🧠 Dynamic Programming Analysis")
    print("=" * 40)
    
    # Count states and transitions
    num_states = len(graph.nodes)
    num_transitions = sum(len(neighbors) for neighbors in graph.adj_list.values())
    
    print(f"📊 Problem Characteristics:")
    print(f"   States (nodes): {num_states}")
    print(f"   Transitions (edges): {num_transitions}") 
    print(f"   State space: O({num_states})")
    print(f"   Transition space: O({num_transitions})")
    
    print(f"\n🔄 DP Properties:")
    print(f"   ✅ Optimal substructure: Shortest path A→C→D requires optimal A→C")
    print(f"   ✅ Overlapping subproblems: Multiple paths may use same intermediate nodes")
    print(f"   ✅ DAG structure: No negative cycles → guaranteed convergence")
    print(f"   ✅ Memoization benefit: O(2^n) → O(n²) complexity reduction")
    
    print(f"\n⚙️  Engineering Implications:")
    print(f"   🚀 Bellman-Ford: O(V×E) = O({num_states}×{num_transitions}) = O({num_states * num_transitions})")
    print(f"   🚀 Dijkstra: O((V+E)logV) = O({num_states + num_transitions}×log{num_states})")
    print(f"   💾 Space complexity: O(V) = O({num_states})")

def run_comprehensive_demo():
    """Run comprehensive DP demonstration"""
    
    print("🚀 DYNAMIC PROGRAMMING SHORTEST PATH DEMO")
    print("=" * 70)
    print("🎯 Goal: Master DP through System Modeling + DAG + Scheduling")
    
    # Test 1: Simple network
    print("\n" + "="*70)
    print("📘 TEST 1: Simple Network (from problem statement)")
    print("="*70)
    
    graph1 = create_test_network()
    graph1.display()
    
    analyze_dp_characteristics(graph1)
    
    # Run both algorithms
    bf_result1 = bellman_ford_shortest_path(graph1, 'A')
    dijk_result1 = dijkstra_shortest_path(graph1, 'A')
    
    # Compare results
    print("\n📊 ALGORITHM COMPARISON")
    print("=" * 30)
    print(f"Bellman-Ford result: {bf_result1}")
    print(f"Dijkstra result:     {dijk_result1}")
    print(f"Results match:       {'✅ YES' if bf_result1 == dijk_result1 else '❌ NO'}")
    
    # Test 2: Complex network  
    print("\n" + "="*70)
    print("📗 TEST 2: Complex Network (multi-path scenarios)")
    print("="*70)
    
    graph2 = create_complex_network()
    graph2.display()
    
    analyze_dp_characteristics(graph2)
    
    # Run both algorithms on complex graph
    bf_result2 = bellman_ford_shortest_path(graph2, 'S') 
    dijk_result2 = dijkstra_shortest_path(graph2, 'S')
    
    print("\n📊 COMPLEX NETWORK RESULTS")
    print("=" * 35)
    print(f"Bellman-Ford: {bf_result2}")
    print(f"Dijkstra:     {dijk_result2}")
    print(f"Match: {'✅ YES' if bf_result2 == dijk_result2 else '❌ NO'}")
    
    # Final insights
    print("\n" + "="*70)
    print("🎓 KEY ENGINEERING INSIGHTS")
    print("="*70)
    print("🔑 DP Core Formula: dp[v] = min(dp[v], dp[u] + weight(u→v))")
    print("🏗️  System Model: DP = DAG + Cache + Optimal Scheduling")
    print("🔄 Bellman-Ford: Eventually consistent (like distributed consensus)")
    print("🎯 Dijkstra: Optimal scheduling (like priority-driven task executor)")
    print("📈 Complexity Gain: Exponential → Polynomial through memoization")
    print("🌐 Applications: TCP optimization, routing protocols, resource allocation")
    
    print(f"\n✨ MISSION ACCOMPLISHED!")
    print(f"You now have the 'DP lens' to model any optimization problem as:")
    print(f"   State Space + Dependency DAG + Transition Rules + Execution Order")

if __name__ == "__main__":
    run_comprehensive_demo()