import { Tabs } from "expo-router";
import React from "react";
import { Text } from "react-native";

function TabIcon({ emoji }: { emoji: string }) {
  return <Text style={{ fontSize: 20 }}>{emoji}</Text>;
}

export default function TabLayout() {
  return (
    <Tabs
      screenOptions={{
        tabBarActiveTintColor: "#3b82f6",
        tabBarInactiveTintColor: "#666",
        tabBarStyle: {
          backgroundColor: "#1a1a2e",
          borderTopColor: "#2a2a3e",
          paddingBottom: 8,
          height: 60,
        },
        tabBarLabelStyle: {
          fontSize: 11,
          fontWeight: "600",
        },
        headerStyle: {
          backgroundColor: "#0f0f1a",
        },
        headerTintColor: "#fff",
        headerTitleStyle: {
          fontWeight: "bold",
        },
      }}
    >
      <Tabs.Screen
        name="index"
        options={{
          title: "Connect",
          tabBarLabel: "Connect",
          tabBarIcon: () => <TabIcon emoji="📡" />,
          headerTitle: "Screentime Headband",
        }}
      />
      <Tabs.Screen
        name="explore"
        options={{
          title: "Graph",
          tabBarLabel: "Graph",
          tabBarIcon: () => <TabIcon emoji="📊" />,
          headerTitle: "Session Graphs",
        }}
      />
      <Tabs.Screen
        name="stats"
        options={{
          title: "Stats",
          tabBarLabel: "Stats",
          tabBarIcon: () => <TabIcon emoji="🔢" />,
          headerTitle: "Statistics",
        }}
      />
    </Tabs>
  );
}
