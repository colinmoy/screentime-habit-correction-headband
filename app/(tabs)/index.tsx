import React, { useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

export default function ConnectScreen() {
  const [status, setStatus] = useState("Not Connected");
  const [connected, setConnected] = useState(false);

  const handleConnect = () => {
    setStatus("Scanning...");
    setTimeout(() => {
      setStatus("Connected to Screentime-Headband");
      setConnected(true);
    }, 2000);
  };

  const handleDisconnect = () => {
    setStatus("Not Connected");
    setConnected(false);
  };

  return (
    <SafeAreaView style={styles.page}>
      <Text style={styles.pageTitle}>Device Connection</Text>

      <View style={styles.card}>
        <Text style={styles.cardLabel}>Device Name</Text>
        <Text style={styles.cardValue}>Screentime-Headband</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardLabel}>Status</Text>
        <Text
          style={[
            styles.cardValue,
            { color: connected ? "#4ade80" : "#f87171" },
          ]}
        >
          {status}
        </Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardLabel}>Protocol</Text>
        <Text style={styles.cardValue}>Bluetooth LE (simulated)</Text>
      </View>

      <TouchableOpacity
        style={[styles.button, connected && styles.buttonDisconnect]}
        onPress={connected ? handleDisconnect : handleConnect}
      >
        <Text style={styles.buttonText}>
          {connected ? "Disconnect" : "Connect"}
        </Text>
      </TouchableOpacity>

      {!connected && (
        <Text style={styles.hint}>
          * Using simulated data for demo purposes
        </Text>
      )}
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  page: {
    flex: 1,
    backgroundColor: "#0f0f1a",
    paddingHorizontal: 16,
  },
  pageTitle: {
    fontSize: 24,
    fontWeight: "bold",
    color: "#fff",
    marginTop: 20,
    marginBottom: 6,
  },
  card: {
    backgroundColor: "#1a1a2e",
    borderRadius: 12,
    padding: 16,
    marginTop: 12,
  },
  cardLabel: {
    color: "#888",
    fontSize: 12,
    marginBottom: 4,
  },
  cardValue: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "600",
  },
  button: {
    backgroundColor: "#3b82f6",
    borderRadius: 12,
    padding: 16,
    alignItems: "center",
    marginTop: 24,
  },
  buttonDisconnect: {
    backgroundColor: "#ef4444",
  },
  buttonText: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "bold",
  },
  hint: {
    color: "#666",
    fontSize: 12,
    textAlign: "center",
    marginTop: 20,
  },
});
