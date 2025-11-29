# Mockups
## Teams Button Box (360x360 Round Display)

### Buttons Required:
- mute/unmute
- cam on/off
- volume up
- volume down
- end call

### Mockup - Circular Layout

```
                    ┌─────────────────────┐
                 ╱                           ╲
              ╱                                 ╲
           ╱           [ VOL + ]                  ╲
         ╱                 ▲                        ╲
       ╱                                              ╲
      │                                                │
     │         [ MUTE ]           [ CAM ]              │
     │            🎤                 📹                 │
     │                                                 │
     │                                                 │
     │                 [ END CALL ]                    │
     │                     ⏹️                          │
     │                   (RED)                         │
     │                                                 │
     │                                                 │
      │                                                │
       ╲                                              ╱
         ╲                 ▼                        ╱
           ╲           [ VOL - ]                  ╱
              ╲                                 ╱
                 ╲                           ╱
                    └─────────────────────┘
```

### Layout Details:

**Top Section (12 o'clock):**
- Volume Up button
- Icon: ▲ or 🔊+
- Position: Center-top arc

**Middle Section (Left & Right, 9-3 o'clock):**
- Mute/Unmute (Left, ~9 o'clock)
  - Icon: 🎤 (unmuted) / 🎤🚫 (muted)
  - Toggle state with color: Green (unmuted) / Red (muted)
  
- Camera On/Off (Right, ~3 o'clock)
  - Icon: 📹 (on) / 📹🚫 (off)
  - Toggle state with color: Green (on) / Gray (off)

**Center Section:**
- End Call button (Large, prominent)
  - Icon: ⏹️ or 📞
  - Color: RED (danger)
  - Size: Larger than other buttons

**Bottom Section (6 o'clock):**
- Volume Down button
- Icon: ▼ or 🔊-
- Position: Center-bottom arc

### Alternative Radial Layout (Mute-Centered)

```
                    ┌─────────────────────┐
                 ╱        [ VOL + ]          ╲
              ╱              🔊                 ╲
           ╱                                      ╲
         ╱                                          ╲
       ╱   [ END CALL ]              [ CAM ]         ╲
      │        📞                       📹            │
     │        (RED)                                   │
     │                                                │
     │              [ MUTE/UNMUTE ]                   │
     │                   🎤                           │
     │              (LARGE CENTER)                    │
     │                                                │
     │                                                │
      │                                               │
       ╲                             [ VOL - ]       ╱
         ╲                              🔉          ╱
           ╲                                      ╱
              ╲                                 ╱
                 ╲                           ╱
                    └─────────────────────┘
```

### Button Specifications:

**Dimensions:**
- Outer buttons: ~80x80px circular buttons
- Center button: ~120x120px circular button
- Button spacing: ~20px from center and edges

**Colors:**
- Mute (Active): Green #00C851
- Mute (Muted): Red #FF4444
- Camera (On): Green #00C851
- Camera (Off): Gray #757575
- Volume: Blue #2196F3
- End Call: Red #CC0000

**Typography:**
- Button labels: 14-16px
- Icons: 32-40px for outer buttons, 48-56px for center
- Status text: 12px (optional, below buttons)

### Interaction States:
- Normal: Default color
- Pressed: Darker shade (20% darker)
- Toggle ON: Bright color
- Toggle OFF: Muted/gray color

### Status Indicators (Optional):
Small text or LED-style dots near buttons to show:
- Mic status: Green dot (unmuted) / Red dot (muted)
- Camera status: Green dot (on) / Gray dot (off)
- Connection status: Small icon in top-right curve 