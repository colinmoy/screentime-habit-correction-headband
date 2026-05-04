import React from "react";
import { Dimensions, ScrollView, StyleSheet, Text, View } from "react-native";
import { BarChart, LineChart } from "react-native-chart-kit";
import { SafeAreaView } from "react-native-safe-area-context";
import { useSession } from "../context/SessionContext";

const screenWidth = Dimensions.get("window").width;

const chartConfigBlue = {
  backgroundGradientFrom: "#1a1a2e",
  backgroundGradientTo: "#1a1a2e",
  color: (opacity = 1) => `rgba(100, 200, 255, ${opacity})`,
  labelColor: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,
  strokeWidth: 2,
  barPercentage: 0.6,
  decimalPlaces: 0,
};

const chartConfigRed = {
  backgroundGradientFrom: "#1a1a2e",
  backgroundGradientTo: "#1a1a2e",
  color: (opacity = 1) => `rgba(255, 100, 100, ${opacity})`,
  labelColor: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,
  strokeWidth: 2,
  barPercentage: 0.6,
  decimalPlaces: 0,
};

const MAX_CHART_SESSIONS = 10;

export default function GraphScreen() {
  const { distanceSessions, postureSessions } = useSession();

  const recentDistance = distanceSessions.slice(-MAX_CHART_SESSIONS);
  const recentPosture = postureSessions.slice(-MAX_CHART_SESSIONS);

  const hasDistance = distanceSessions.length > 0;
  const hasPosture = postureSessions.length > 0;

  // empty state
  if (!hasDistance && !hasPosture) {
    return (
      <SafeAreaView style={styles.page}>
        <Text style={styles.pageTitle}>Session Graphs</Text>
        <View style={styles.emptyContainer}>
          <Text style={styles.emptyIcon}>📊</Text>
          <Text style={styles.emptyTitle}>No Sessions Yet</Text>
          <Text style={styles.emptySubtitle}>
            Connect to your ESP32 on the Connect tab to start recording sessions
          </Text>
        </View>
      </SafeAreaView>
    );
  }

  const distanceLabels = recentDistance.map((s) => `D${s.id}`);
  const distanceData = recentDistance.map((s) => s.time);

  const postureLabels = recentPosture.map((s) => `P${s.id}`);
  const postureData = recentPosture.map((s) => s.time);

  // combined line chart needs same length — use whichever is longer
  // pad the shorter one with 0s if lengths differ
  const combinedLength = Math.max(recentDistance.length, recentPosture.length);
  const combinedLabels = Array.from(
    { length: combinedLength },
    (_, i) => `${i + 1}`,
  );
  const paddedDistance = [
    ...distanceData,
    ...Array(combinedLength - distanceData.length).fill(0),
  ];
  const paddedPosture = [
    ...postureData,
    ...Array(combinedLength - postureData.length).fill(0),
  ];

  return (
    <SafeAreaView style={styles.page}>
      <ScrollView showsVerticalScrollIndicator={false}>
        <Text style={styles.pageTitle}>Session Graphs</Text>
        <Text style={styles.subtitle}>
          Time (seconds) of good behavior before alarm
        </Text>

        {/* Distance Bar Chart */}
        {hasDistance && (
          <>
            <Text style={styles.chartTitle}>
              📏 Distance — Last {recentDistance.length} of{" "}
              {distanceSessions.length} sessions
            </Text>
            <BarChart
              data={{
                labels: distanceLabels,
                datasets: [{ data: distanceData }],
              }}
              width={screenWidth - 32}
              height={200}
              chartConfig={chartConfigRed}
              style={styles.chart}
              showValuesOnTopOfBars
              fromZero
              yAxisLabel=""
              yAxisSuffix="s"
            />
          </>
        )}

        {/* Posture Bar Chart */}
        {hasPosture && (
          <>
            <Text style={styles.chartTitle}>
              ⬆ Posture — Last {recentPosture.length} of{" "}
              {postureSessions.length} sessions
            </Text>
            <BarChart
              data={{
                labels: postureLabels,
                datasets: [{ data: postureData }],
              }}
              width={screenWidth - 32}
              height={200}
              chartConfig={chartConfigBlue}
              style={styles.chart}
              showValuesOnTopOfBars
              fromZero
              yAxisLabel=""
              yAxisSuffix="s"
            />
          </>
        )}

        {/* Combined Line Chart — only show if both sensors have data */}
        {hasDistance && hasPosture && (
          <>
            <Text style={styles.chartTitle}>📈 Combined Timeline</Text>
            <LineChart
              data={{
                labels: combinedLabels,
                datasets: [
                  {
                    data: paddedDistance,
                    color: () => "rgba(255, 100, 100, 1)",
                    strokeWidth: 2,
                  },
                  {
                    data: paddedPosture,
                    color: () => "rgba(100, 200, 255, 1)",
                    strokeWidth: 2,
                  },
                ],
                legend: ["Distance", "Posture"],
              }}
              width={screenWidth - 32}
              height={220}
              chartConfig={chartConfigBlue}
              style={styles.chart}
              bezier
              yAxisLabel=""
              yAxisSuffix="s"
            />
          </>
        )}

        {/* Only one sensor connected — show note */}
        {hasDistance && !hasPosture && (
          <View style={styles.noSensorCard}>
            <Text style={styles.noSensorText}>
              ⬆ Posture chart will appear once posture sensor data is recorded
            </Text>
          </View>
        )}
        {hasPosture && !hasDistance && (
          <View style={styles.noSensorCard}>
            <Text style={styles.noSensorText}>
              📏 Distance chart will appear once distance sensor data is
              recorded
            </Text>
          </View>
        )}

        {/* Legend */}
        <View style={styles.legend}>
          {hasDistance && (
            <View style={styles.legendItem}>
              <View
                style={[styles.legendDot, { backgroundColor: "#ff6464" }]}
              />
              <Text style={styles.legendText}>Distance</Text>
            </View>
          )}
          {hasPosture && (
            <View style={styles.legendItem}>
              <View
                style={[styles.legendDot, { backgroundColor: "#64c8ff" }]}
              />
              <Text style={styles.legendText}>Posture</Text>
            </View>
          )}
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
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 13,
    color: "#aaa",
    marginBottom: 16,
  },
  chartTitle: {
    fontSize: 14,
    fontWeight: "600",
    color: "#ccc",
    marginTop: 20,
    marginBottom: 6,
  },
  chart: {
    borderRadius: 12,
  },
  legend: {
    flexDirection: "row",
    justifyContent: "center",
    gap: 24,
    marginTop: 12,
  },
  legendItem: {
    flexDirection: "row",
    alignItems: "center",
    gap: 6,
  },
  legendDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  legendText: {
    color: "#aaa",
    fontSize: 13,
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
    marginTop: 16,
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
