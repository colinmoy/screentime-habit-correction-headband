import React, { useState } from "react";
import {
  Alert,
  KeyboardAvoidingView,
  Platform,
  ScrollView,
  StyleSheet,
  Text,
  TextInput,
  TouchableOpacity,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { useSession } from "../context/SessionContext";

export default function ConnectScreen() {
  const {
    status,
    ipAddress,
    setIpAddress,
    connect,
    disconnect,
    clearSessions,
    distanceSessions,
    postureSessions,
  } = useSession();
  const [inputIP, setInputIP] = useState(ipAddress);

  const connected = status === "connected";
  const connecting = status === "connecting";

  const handleConnect = () => {
    if (!inputIP.trim()) {
      Alert.alert("Missing IP", "Please enter the ESP32 IP address first.");
      return;
    }
    setIpAddress(inputIP.trim());
    connect();
  };

  const handleClearSessions = () => {
    Alert.alert(
      "Clear Sessions",
      "Are you sure you want to delete all session data?",
      [
        { text: "Cancel", style: "cancel" },
        { text: "Clear", style: "destructive", onPress: clearSessions },
      ],
    );
  };

  const statusColor = () => {
    if (status === "connected") return "#4ade80";
    if (status === "connecting") return "#facc15";
    if (status === "error") return "#f87171";
    return "#f87171";
  };

  const statusLabel = () => {
    if (status === "connected") return "Connected";
    if (status === "connecting") return "Connecting...";
    if (status === "error") return "Connection Failed";
    return "Not Connected";
  };

  const totalSessions = distanceSessions.length + postureSessions.length;

  return (
    <SafeAreaView style={styles.page}>
      <KeyboardAvoidingView
        behavior={Platform.OS === "ios" ? "padding" : undefined}
        style={{ flex: 1 }}
      >
        <ScrollView showsVerticalScrollIndicator={false}>
          <Text style={styles.pageTitle}>Device Connection</Text>
          <Text style={styles.subtitle}>Connect to your ESP32 over Wi-Fi</Text>

          {/* Status Card */}
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Status</Text>
            <Text style={[styles.cardValue, { color: statusColor() }]}>
              {statusLabel()}
            </Text>
          </View>

          {/* IP Input */}
          <View style={styles.card}>
            <Text style={styles.cardLabel}>ESP32 IP Address</Text>
            <TextInput
              style={styles.input}
              value={inputIP}
              onChangeText={setInputIP}
              placeholder="192.168.4.1"
              placeholderTextColor="#555"
              keyboardType="decimal-pad"
              editable={!connected && !connecting}
              autoCapitalize="none"
              autoCorrect={false}
            />
            <Text style={styles.hint}>
              Connect your phone to the Headband-AP hotspot then enter
              192.168.4.1
            </Text>
          </View>

          {/* Device Info */}
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Device Name</Text>
            <Text style={styles.cardValue}>Screentime-Headband</Text>
          </View>

          <View style={styles.card}>
            <Text style={styles.cardLabel}>Protocol</Text>
            <Text style={styles.cardValue}>Wi-Fi HTTP (polling every 3s)</Text>
          </View>

          {/* Sessions Stored — split by sensor */}
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Sessions Stored</Text>
            <Text style={[styles.cardValue, { color: "#ff6464" }]}>
              📏 Distance: {distanceSessions.length} / 100
            </Text>
            <Text
              style={[styles.cardValue, { color: "#64c8ff", marginTop: 6 }]}
            >
              ⬆ Posture: {postureSessions.length} / 100
            </Text>
          </View>

          {/* Connect / Disconnect Button */}
          <TouchableOpacity
            style={[
              styles.button,
              connected && styles.buttonDisconnect,
              connecting && styles.buttonConnecting,
            ]}
            onPress={connected ? disconnect : handleConnect}
            disabled={connecting}
          >
            <Text style={styles.buttonText}>
              {connected
                ? "Disconnect"
                : connecting
                  ? "Connecting..."
                  : "Connect"}
            </Text>
          </TouchableOpacity>

          {/* Clear Sessions Button — only show if any sessions exist */}
          {totalSessions > 0 && (
            <TouchableOpacity
              style={styles.buttonClear}
              onPress={handleClearSessions}
            >
              <Text style={styles.buttonClearText}>Clear All Sessions</Text>
            </TouchableOpacity>
          )}

          <View style={{ height: 40 }} />
        </ScrollView>
      </KeyboardAvoidingView>
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
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 13,
    color: "#aaa",
    marginBottom: 16,
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
    marginBottom: 6,
  },
  cardValue: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "600",
  },
  input: {
    backgroundColor: "#0f0f1a",
    color: "#fff",
    fontSize: 16,
    borderRadius: 8,
    padding: 10,
    borderWidth: 1,
    borderColor: "#2a2a3e",
  },
  hint: {
    color: "#555",
    fontSize: 11,
    marginTop: 6,
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
  buttonConnecting: {
    backgroundColor: "#64748b",
  },
  buttonText: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "bold",
  },
  buttonClear: {
    borderRadius: 12,
    padding: 16,
    alignItems: "center",
    marginTop: 12,
    borderWidth: 1,
    borderColor: "#ef4444",
  },
  buttonClearText: {
    color: "#ef4444",
    fontSize: 14,
    fontWeight: "600",
  },
});
