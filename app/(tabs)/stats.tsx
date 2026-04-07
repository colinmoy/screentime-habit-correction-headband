import React from "react";
import { ScrollView, StyleSheet, Text, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import {
    average,
    FAKE_SESSIONS,
    maxVal,
    median,
    minVal,
} from "../../fakeSessions";

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

export default function StatsScreen() {
  const postureData = FAKE_SESSIONS.map((s) => s.timeToPostureAlarm);
  const distanceData = FAKE_SESSIONS.map((s) => s.timeToDistanceAlarm);

  return (
    <SafeAreaView style={styles.page}>
      <ScrollView showsVerticalScrollIndicator={false}>
        <Text style={styles.pageTitle}>Raw Data & Statistics</Text>

        {/* Posture Stats */}
        <Text style={styles.sectionTitle}>⬆ Posture Alarm Times</Text>
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

        {/* Distance Stats */}
        <Text style={styles.sectionTitle}>📏 Distance Alarm Times</Text>
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

        {/* Raw Session Log */}
        <Text style={styles.sectionTitle}>📋 Session Log</Text>
        <View style={styles.table}>
          <View style={styles.tableRow}>
            <Text style={[styles.tableCell, styles.tableHeader]}>Session</Text>
            <Text style={[styles.tableCell, styles.tableHeader]}>
              Posture (s)
            </Text>
            <Text style={[styles.tableCell, styles.tableHeader]}>
              Distance (s)
            </Text>
          </View>
          {FAKE_SESSIONS.map((s) => (
            <View key={s.id} style={styles.tableRow}>
              <Text style={styles.tableCell}>{s.label}</Text>
              <Text style={[styles.tableCell, { color: "#64c8ff" }]}>
                {s.timeToPostureAlarm}
              </Text>
              <Text style={[styles.tableCell, { color: "#ff6464" }]}>
                {s.timeToDistanceAlarm}
              </Text>
            </View>
          ))}
        </View>

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
    marginBottom: 6,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: "600",
    color: "#fff",
    marginTop: 24,
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
    padding: 12,
    color: "#fff",
    fontSize: 13,
    textAlign: "center",
  },
  tableHeader: {
    color: "#888",
    fontWeight: "600",
    backgroundColor: "#12122a",
  },
});
