import React from "react";
import { Dimensions, ScrollView, StyleSheet, Text, View } from "react-native";
import { BarChart, LineChart } from "react-native-chart-kit";
import { SafeAreaView } from "react-native-safe-area-context";
import { FAKE_SESSIONS } from "../../fakeSessions";

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

export default function GraphScreen() {
  const labels = FAKE_SESSIONS.map((s) => s.label);
  const postureData = FAKE_SESSIONS.map((s) => s.timeToPostureAlarm);
  const distanceData = FAKE_SESSIONS.map((s) => s.timeToDistanceAlarm);

  return (
    <SafeAreaView style={styles.page}>
      <ScrollView showsVerticalScrollIndicator={false}>
        <Text style={styles.pageTitle}>Session Graphs</Text>
        <Text style={styles.subtitle}>
          Time (seconds) before alarm triggered
        </Text>

        {/* Posture Bar Chart */}
        <Text style={styles.chartTitle}>⬆ Posture — Time to Alarm (s)</Text>
        <BarChart
          data={{ labels, datasets: [{ data: postureData }] }}
          width={screenWidth - 32}
          height={200}
          chartConfig={chartConfigBlue}
          style={styles.chart}
          showValuesOnTopOfBars
          fromZero
          yAxisLabel=""
          yAxisSuffix="s"
        />

        {/* Distance Bar Chart */}
        <Text style={styles.chartTitle}>📏 Distance — Time to Alarm (s)</Text>
        <BarChart
          data={{ labels, datasets: [{ data: distanceData }] }}
          width={screenWidth - 32}
          height={200}
          chartConfig={chartConfigRed}
          style={styles.chart}
          showValuesOnTopOfBars
          fromZero
          yAxisLabel=""
          yAxisSuffix="s"
        />

        {/* Combined Line Chart */}
        <Text style={styles.chartTitle}>📈 Combined Timeline</Text>
        <LineChart
          data={{
            labels,
            datasets: [
              {
                data: postureData,
                color: () => "rgba(100, 200, 255, 1)",
                strokeWidth: 2,
              },
              {
                data: distanceData,
                color: () => "rgba(255, 100, 100, 1)",
                strokeWidth: 2,
              },
            ],
            legend: ["Posture", "Distance"],
          }}
          width={screenWidth - 32}
          height={220}
          chartConfig={chartConfigBlue}
          style={styles.chart}
          bezier
          yAxisLabel=""
          yAxisSuffix="s"
        />

        <View style={styles.legend}>
          <View style={styles.legendItem}>
            <View style={[styles.legendDot, { backgroundColor: "#64c8ff" }]} />
            <Text style={styles.legendText}>Posture</Text>
          </View>
          <View style={styles.legendItem}>
            <View style={[styles.legendDot, { backgroundColor: "#ff6464" }]} />
            <Text style={styles.legendText}>Distance</Text>
          </View>
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
});
