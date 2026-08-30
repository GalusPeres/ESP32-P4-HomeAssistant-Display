const fs = require('fs');
const path = require('path');

// Pfad zur SCSS Variables Datei von @mdi/font
const scssPath = path.join(__dirname, 'node_modules', '@mdi', 'font', 'scss', '_variables.scss');

console.log('🔍 Reading MDI variables...');

// Lese die SCSS Datei
const scssContent = fs.readFileSync(scssPath, 'utf8');

// Parse die Icon-Definitionen
// Format: "icon-name": F0XXX,
const iconRegex = /"([\w-]+)":\s*([0-9A-Fa-f]+),?/g;
const icons = [];

let match;
while ((match = iconRegex.exec(scssContent)) !== null) {
  const iconName = match[1];
  const codepoint = match[2];
  icons.push({ name: iconName, codepoint: codepoint.toUpperCase() });
}

console.log(`✅ Found ${icons.length} icons!`);

// ALLE Icons ausgeben (keine Filterung)
console.log(`🎯 Exporting all ${icons.length} icons`);

// Ausgabe als C++ Map
console.log('\n📋 C++ map entries:\n');
icons.forEach(icon => {
  console.log(`  {"${icon.name}", 0x${icon.codepoint}},`);
});

// Speichere in Datei
fs.writeFileSync('icons.txt', icons.map(i => `{"${i.name}", 0x${i.codepoint}},`).join('\n'));
console.log('\n💾 Saved to icons.txt');
