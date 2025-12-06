const sharp = require('sharp');

// Erstelle ein 64x64 Icon mit "T5" Text
const svg = `
<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
  <rect width="64" height="64" rx="8" fill="#3b82f6"/>
  <text x="32" y="45" font-family="Arial" font-size="32" font-weight="bold"
        text-anchor="middle" fill="white">T5</text>
</svg>
`;

sharp(Buffer.from(svg))
  .png()
  .toFile('icon.png')
  .then(() => console.log('✅ icon.png created'))
  .catch(err => console.error('❌ Error:', err));
