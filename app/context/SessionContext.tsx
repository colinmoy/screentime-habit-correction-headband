import AsyncStorage from "@react-native-async-storage/async-storage";
import React, {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";

// ─── Types ────────────────────────────────────────────────────────────────────
export type Session = {
  id: number;
  timestamp: string;
  time: number; // seconds of good behavior before alarm
};

type ConnectionStatus = "disconnected" | "connecting" | "connected" | "error";

type SessionContextType = {
  distanceSessions: Session[];
  postureSessions: Session[];
  status: ConnectionStatus;
  ipAddress: string;
  setIpAddress: (ip: string) => void;
  connect: () => void;
  disconnect: () => void;
  clearSessions: () => void;
};

// ─── Constants ────────────────────────────────────────────────────────────────
const MAX_SESSIONS = 100;
const POLL_INTERVAL_MS = 3000;
const DISTANCE_KEY = "distance_sessions";
const POSTURE_KEY = "posture_sessions";

// ─── Context ──────────────────────────────────────────────────────────────────
const SessionContext = createContext<SessionContextType | null>(null);

export function useSession() {
  const ctx = useContext(SessionContext);
  if (!ctx) throw new Error("useSession must be used within SessionProvider");
  return ctx;
}

// ─── Provider ─────────────────────────────────────────────────────────────────
export function SessionProvider({ children }: { children: React.ReactNode }) {
  const [distanceSessions, setDistanceSessions] = useState<Session[]>([]);
  const [postureSessions, setPostureSessions] = useState<Session[]>([]);
  const [status, setStatus] = useState<ConnectionStatus>("disconnected");
  const [ipAddress, setIpAddress] = useState("");

  const pollRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const distanceIdRef = useRef(1);
  const postureIdRef = useRef(1);

  // ─── Load saved sessions on startup ────────────────────────────────────────
  useEffect(() => {
    async function loadSessions() {
      try {
        const distStored = await AsyncStorage.getItem(DISTANCE_KEY);
        const postStored = await AsyncStorage.getItem(POSTURE_KEY);

        if (distStored) {
          const parsed: Session[] = JSON.parse(distStored);
          setDistanceSessions(parsed);
          if (parsed.length > 0)
            distanceIdRef.current = parsed[parsed.length - 1].id + 1;
        }
        if (postStored) {
          const parsed: Session[] = JSON.parse(postStored);
          setPostureSessions(parsed);
          if (parsed.length > 0)
            postureIdRef.current = parsed[parsed.length - 1].id + 1;
        }
      } catch (e) {
        console.error("Failed to load sessions:", e);
      }
    }
    loadSessions();
  }, []);

  // ─── Save sessions whenever they change ────────────────────────────────────
  useEffect(() => {
    AsyncStorage.setItem(DISTANCE_KEY, JSON.stringify(distanceSessions));
  }, [distanceSessions]);

  useEffect(() => {
    AsyncStorage.setItem(POSTURE_KEY, JSON.stringify(postureSessions));
  }, [postureSessions]);

  // ─── Add session helpers ───────────────────────────────────────────────────
  const addDistanceSession = useCallback((time: number) => {
    const newSession: Session = {
      id: distanceIdRef.current++,
      timestamp: new Date().toISOString(),
      time,
    };
    setDistanceSessions((prev) => {
      const updated = [...prev, newSession];
      return updated.length > MAX_SESSIONS
        ? updated.slice(updated.length - MAX_SESSIONS)
        : updated;
    });
  }, []);

  const addPostureSession = useCallback((time: number) => {
    const newSession: Session = {
      id: postureIdRef.current++,
      timestamp: new Date().toISOString(),
      time,
    };
    setPostureSessions((prev) => {
      const updated = [...prev, newSession];
      return updated.length > MAX_SESSIONS
        ? updated.slice(updated.length - MAX_SESSIONS)
        : updated;
    });
  }, []);

  // ─── Poll ESP32 independently for each sensor ──────────────────────────────
  const pollESP32 = useCallback(async () => {
    try {
      // poll distance endpoint
      const distRes = await fetch(`http://${ipAddress}/distance`);
      if (distRes.ok) {
        const distData = await distRes.json();
        if (distData.newSession && distData.distanceTime !== null) {
          addDistanceSession(distData.distanceTime);
        }
      }

      // poll posture endpoint independently
      const postRes = await fetch(`http://${ipAddress}/posture`);
      if (postRes.ok) {
        const postData = await postRes.json();
        if (postData.newSession && postData.postureTime !== null) {
          addPostureSession(postData.postureTime);
        }
      }
    } catch (e) {
      console.warn("Poll failed:", e);
      setStatus("error");
      stopPolling();
    }
  }, [ipAddress, addDistanceSession, addPostureSession]);

  // ─── Start/stop polling ────────────────────────────────────────────────────
  const startPolling = useCallback(() => {
    if (pollRef.current) return;
    pollRef.current = setInterval(pollESP32, POLL_INTERVAL_MS);
  }, [pollESP32]);

  const stopPolling = useCallback(() => {
    if (pollRef.current) {
      clearInterval(pollRef.current);
      pollRef.current = null;
    }
  }, []);

  // ─── Connect ───────────────────────────────────────────────────────────────
  const connect = useCallback(async () => {
    if (!ipAddress) return;
    setStatus("connecting");
    try {
      const response = await fetch(`http://${ipAddress}/ping`);
      if (response.ok) {
        setStatus("connected");
        startPolling();
      } else {
        setStatus("error");
      }
    } catch (e) {
      setStatus("error");
    }
  }, [ipAddress, startPolling]);

  // ─── Disconnect ────────────────────────────────────────────────────────────
  const disconnect = useCallback(() => {
    stopPolling();
    setStatus("disconnected");
  }, [stopPolling]);

  // ─── Clear all sessions ────────────────────────────────────────────────────
  const clearSessions = useCallback(() => {
    setDistanceSessions([]);
    setPostureSessions([]);
    distanceIdRef.current = 1;
    postureIdRef.current = 1;
  }, []);

  // ─── Cleanup on unmount ────────────────────────────────────────────────────
  useEffect(() => {
    return () => stopPolling();
  }, [stopPolling]);

  return (
    <SessionContext.Provider
      value={{
        distanceSessions,
        postureSessions,
        status,
        ipAddress,
        setIpAddress,
        connect,
        disconnect,
        clearSessions,
      }}
    >
      {children}
    </SessionContext.Provider>
  );
}
