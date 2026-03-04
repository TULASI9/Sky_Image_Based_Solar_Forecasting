import express from "express";
import mongoose from "mongoose";
import multer from "multer";
import bodyParser from "body-parser";
import dotenv from "dotenv";
import cors from "cors";
import axios from "axios";

dotenv.config();

const app = express();
const PORT = process.env.PORT || 5000;

app.use(cors({ origin: "*" }));
app.use(bodyParser.json());



// =============================
// Multer setup
// =============================
const storage = multer.memoryStorage();
const upload = multer({ storage });

// =============================
// MongoDB connection
// =============================
mongoose
  .connect(process.env.MONGO_URI, {
    useNewUrlParser: true,
    useUnifiedTopology: true,
    dbName: "pv_forecast",
  })
  .then(() => console.log("✅ MongoDB connected"))
  .catch((err) => console.error("❌ MongoDB connection error:", err));

mongoose.connection.on("connected", () => {
  console.log(`✅ MongoDB connected to database: ${mongoose.connection.name}`);
});



// =============================
// Schema (PV as Decimal128)
// =============================
const docSchema = new mongoose.Schema({
  image: Buffer,
  pv: mongoose.Schema.Types.Decimal128,
  createdAt: { type: Date, default: Date.now },
});

const Doc = mongoose.model("Doc", docSchema, "docs");

// =============================
// Upload API
// =============================


app.post("/upload", upload.single("image"), async (req, res) => {
  try {
    const { pv } = req.body;
    if (!req.file) {
      return res.status(400).json({ error: 1 }); // 1 = missing image
    }

    const latestDoc = await Doc.findOne().sort({ createdAt: -1 });

    if (latestDoc) {
      const timeDiff =
        (Date.now() - new Date(latestDoc.createdAt).getTime()) / 60000;

      if (timeDiff > 7) {
        await Doc.deleteMany({});
        console.log("⚠️ Gap exceeded 7 min → queue cleared");
      }
    }

    const newDoc = new Doc({
      image: req.file.buffer,
      pv: mongoose.Types.Decimal128.fromString(parseFloat(pv).toString()),
    });
    await newDoc.save();

    const count = await Doc.countDocuments();
    if (count > 8) {
      const oldest = await Doc.find().sort({ createdAt: 1 }).limit(1);
      await Doc.deleteOne({ _id: oldest[0]._id });
      console.log("♻️ Queue exceeded 8 → oldest removed");
    }

    const finalCount = await Doc.countDocuments();

    // Trigger prediction if queue is full
    let prediction = null;
    if (finalCount === 8) {
      try {
        const response = await axios.get("http://localhost:8000/predict");
        prediction = Math.round(response.data.prediction_real); // ✅ integer real value
        console.log("✅ Prediction fetched:", prediction);
      } catch (err) {
        console.error("❌ Failed to fetch prediction:", err.message);
      }
    }

    res.json({
      success: 1,
      queueSize: finalCount,
      ...(finalCount === 8 && { prediction: prediction || -1 }),
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 2 }); // 2 = upload failed
  }
});

// =============================
// Get most recent document
// =============================
app.get("/latest", async (req, res) => {
  try {
    const doc = await Doc.findOne().sort({ createdAt: -1 });
    if (!doc) return res.status(404).json({ error: 3 }); // 3 = no document

    res.json({
      pv: doc.pv ? Math.round(parseFloat(doc.pv.toString())) : -1,
      image: doc.image ? doc.image.toString("base64") : null, // ✅ add base64 image
      createdAt: Math.floor(new Date(doc.createdAt).getTime() / 1000), // epoch seconds
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 4 }); // 4 = fetch failed
  }
});

// =============================
// Get latest prediction (last queue full result)
// =============================
app.get("/latest-prediction", async (req, res) => {
  try {
    // Find latest doc
    const latestDoc = await Doc.findOne().sort({ createdAt: -1 });
    if (!latestDoc) {
      return res.status(404).json({ error: "No documents available" });
    }

    // Only trigger prediction if queue is full
    const count = await Doc.countDocuments();
    if (count < 8) {
      return res.json({ message: "Queue not full yet", prediction: null });
    }

    try {
      const response = await axios.get("http://localhost:8000/predict");
      const prediction = parseInt(response.data.prediction_real); // integer for Arduino
      res.json({ prediction });
    } catch (err) {
      res.status(500).json({ error: "Prediction service unavailable" });
    }
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch prediction" });
  }
});

// =============================
// Health-check / Root route
// =============================
app.get("/", (req, res) => {
  res.json({ status: 1 }); // 1 = server running
});

// =============================
// Start server
// =============================
app.listen(PORT, () =>
  console.log(`🚀 Server running at http://localhost:${PORT}`)
);
