import React, { useState } from "react";
import {
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { Session, useSession } from "../context/SessionContext";

// ─── Stat Helpers ─────────────────────────────────────────────────────────────
function average(arr: number[]): string {
  if (arr.length === 0) return "N/A";
  return (arr.reduce((a, b) => a + b, 0) / arr.length).toFixed(1);
}

function median(arr: number[]): string {
  if (arr.length === 0) return "N/A";
  const sorted = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 !== 0
    ? sorted[mid].toString()
    : ((sorted[mid - 1] + sorted[mid]) / 2).toFixed(1);
}

function minVal(arr: number[]): string {
  if (arr.length === 0) return "N/A";
  return Math.min(...arr).toString();
}

function maxVal(arr: number[]): string {
  if (arr.length === 0) return "N/A";
  return Math.max(...arr).toString();
}

function formatTimestamp(iso: string): string {
  const date = new Date(iso);
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

// ─── Stat Box ─────────────────────────────────────────────────────────────────
function StatBox({
  label,
  value,
  color,
}: {
  label: string;
  value: string;
  color: string;
}) {
  return (
    <View style={styles.statBox}>
      <Text style={styles.statLabel}>{label}</Text>
      <Text style={[styles.statValue, { color }]}>{value}</Text>
    </View>
  );
}

// ─── Session Table ────────────────────────────────────────────────────────────
function SessionTable({
  sessions,
  color,
  label,
}: {
  sessions: Session[];
  color: string;
  label: string;
}) {
  if (sessions.length === 0) {
    return (
      <View style={styles.noSensorCard}>
        <Text style={styles.noSensorText}>
          No {label} sessions recorded yet
        </Text>
      </View>
    );
  }

  return (
    <View style={styles.table}>
      <View style={styles.tableRow}>
        <Text style={[styles.tableCell, styles.tableHeader, { flex: 0.7 }]}>
          #
        </Text>
        <Text style={[styles.tableCell, styles.tableHeader]}>Time</Text>
        <Text style={[styles.tableCell, styles.tableHeader]}>Good (s)</Text>
      </View>
      {[...sessions].reverse().map((s) => (
        <View key={s.id} style={styles.tableRow}>
          <Text style={[styles.tableCell, { flex: 0.7, color: "#888" }]}>
            {s.id}
          </Text>
          <Text style={[styles.tableCell, { color: "#aaa", fontSize: 11 }]}>
            {formatTimestamp(s.timestamp)}
          </Text>
          <Text style={[styles.tableCell, { color }]}>{s.time}</Text>
        </View>
      ))}
    </View>
  );
}

// ─── Stats Screen ─────────────────────────────────────────────────────────────
export default function StatsScreen() {
  const { distanceSessions, postureSessions } = useSession();
  const [activeTab, setActiveTab] = useState<"distance" | "posture">(
    "distance",
  );

  const distanceData = distanceSessions.map((s) => s.time);
  const postureData = postureSessions.map((s) => s.time);

  const hasDistance = distanceSessions.length > 0;
  const hasPosture = postureSessions.length > 0;

  // empty state
  if (!hasDistance && !hasPosture) {
    return (
      <SafeAreaView style={styles.page}>
        <Text style={styles.pageTitle}>Raw Data & Statistics</Text>
        <View style={styles.emptyContainer}>
          <Text style={styles.emptyIcon}>🔢</Text>
          <Text style={styles.emptyTitle}>No Sessions Yet</Text>
          <Text style={styles.emptySubtitle}>
            Connect to your ESP32 on the Connect tab to start recording sessions
          </Text>
        </View>
      </SafeAreaView>
    );
  }

  return (
    <SafeAreaView style={styles.page}>
      <ScrollView showsVerticalScrollIndicator={false}>
        <Text style={styles.pageTitle}>Raw Data & Statistics</Text>

        {/* Tab Switcher */}
        <View style={styles.tabSwitcher}>
          <TouchableOpacity
            style={[
              styles.switchTab,
              activeTab === "distance" && styles.switchTabActive,
            ]}
            onPress={() => setActiveTab("distance")}
          >
            <Text
              style={[
                styles.switchTabText,
                activeTab === "distance" && { color: "#ff6464" },
              ]}
            >
              📏 Distance
            </Text>
            <Text style={styles.switchTabCount}>
              {distanceSessions.length} / 100
            </Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[
              styles.switchTab,
              activeTab === "posture" && styles.switchTabActive,
            ]}
            onPress={() => setActiveTab("posture")}
          >
            <Text
              style={[
                styles.switchTabText,
                activeTab === "posture" && { color: "#64c8ff" },
              ]}
            >
              ⬆ Posture
            </Text>
            <Text style={styles.switchTabCount}>
              {postureSessions.length} / 100
            </Text>
          </TouchableOpacity>
        </View>

        {/* Distance Stats */}
        {activeTab === "distance" && (
          <>
            <Text style={styles.sectionTitle}>📏 Distance Statistics</Text>
            <View style={styles.statsGrid}>
              <StatBox
                label="Average"
                value={`${average(distanceData)}s`}
                color="#ff6464"
              />
              <StatBox
                label="Median"
                value={`${median(distanceData)}s`}
                color="#ff6464"
              />
              <StatBox
                label="Min"
                value={`${minVal(distanceData)}s`}
                color="#ff6464"
              />
              <StatBox
                label="Max"
                value={`${maxVal(distanceData)}s`}
                color="#ff6464"
              />
            </View>
            <Text style={styles.sectionTitle}>📋 Distance Session Log</Text>
            <SessionTable
              sessions={distanceSessions}
              color="#ff6464"
              label="distance"
            />
          </>
        )}

        {/* Posture Stats */}
        {activeTab === "posture" && (
          <>
            <Text style={styles.sectionTitle}>⬆ Posture Statistics</Text>
            <View style={styles.statsGrid}>
              <StatBox
                label="Average"
                value={`${average(postureData)}s`}
                color="#64c8ff"
              />
              <StatBox
                label="Median"
                value={`${median(postureData)}s`}
                color="#64c8ff"
              />
              <StatBox
                label="Min"
                value={`${minVal(postureData)}s`}
                color="#64c8ff"
              />
              <StatBox
                label="Max"
                value={`${maxVal(postureData)}s`}
                color="#64c8ff"
              />
            </View>
            <Text style={styles.sectionTitle}>📋 Posture Session Log</Text>
            <SessionTable
              sessions={postureSessions}
              color="#64c8ff"
              label="posture"
            />
          </>
        )}

        <View style={{ height: 40 }} />
      </ScrollView>
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
    marginBottom: 16,
  },
  tabSwitcher: {
    flexDirection: "row",
    backgroundColor: "#1a1a2e",
    borderRadius: 12,
    padding: 4,
    marginBottom: 8,
  },
  switchTab: {
    flex: 1,
    alignItems: "center",
    paddingVertical: 10,
    borderRadius: 10,
  },
  switchTabActive: {
    backgroundColor: "#0f0f1a",
  },
  switchTabText: {
    color: "#666",
    fontSize: 14,
    fontWeight: "600",
  },
  switchTabCount: {
    color: "#555",
    fontSize: 11,
    marginTop: 2,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: "600",
    color: "#fff",
    marginTop: 20,
    marginBottom: 10,
  },
  statsGrid: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: 10,
  },
  statBox: {
    backgroundColor: "#1a1a2e",
    borderRadius: 12,
    padding: 14,
    width: "47%",
    alignItems: "center",
  },
  statLabel: {
    color: "#888",
    fontSize: 12,
    marginBottom: 4,
  },
  statValue: {
    fontSize: 20,
    fontWeight: "bold",
  },
  table: {
    backgroundColor: "#1a1a2e",
    borderRadius: 12,
    overflow: "hidden",
    marginBottom: 20,
  },
  tableRow: {
    flexDirection: "row",
    borderBottomWidth: 1,
    borderBottomColor: "#2a2a3e",
  },
  tableCell: {
    flex: 1,
    padding: 10,
    color: "#fff",
    fontSize: 13,
    textAlign: "center",
  },
  tableHeader: {
    color: "#888",
    fontWeight: "600",
    backgroundColor: "#12122a",
    fontSize: 12,
  },
  emptyContainer: {
    flex: 1,
    justifyContent: "center",
    alignItems: "center",
    paddingTop: 100,
  },
  emptyIcon: {
    fontSize: 48,
    marginBottom: 16,
  },
  emptyTitle: {
    fontSize: 20,
    fontWeight: "bold",
    color: "#fff",
    marginBottom: 8,
  },
  emptySubtitle: {
    fontSize: 14,
    color: "#888",
    textAlign: "center",
    paddingHorizontal: 32,
  },
  noSensorCard: {
    backgroundColor: "#1a1a2e",
    borderRadius: 12,
    padding: 16,
    marginTop: 8,
    borderWidth: 1,
    borderColor: "#2a2a3e",
    borderStyle: "dashed",
  },
  noSensorText: {
    color: "#555",
    fontSize: 13,
    textAlign: "center",
  },
});
