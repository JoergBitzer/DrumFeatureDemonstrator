const vscode = require('vscode');

// Matches JUCE's 0xAARRGGBB hex literals, e.g. 0xffd01818
const HEX_ARGB = /\b0x([0-9a-fA-F]{8})\b/g;

const toByte = (v) => Math.round(v * 255).toString(16).padStart(2, '0');

const provider = {
    provideDocumentColors(document) {
        const text = document.getText();
        const results = [];
        let match;
        HEX_ARGB.lastIndex = 0;
        while ((match = HEX_ARGB.exec(text)) !== null) {
            const hex = match[1];
            const a = parseInt(hex.substring(0, 2), 16) / 255;
            const r = parseInt(hex.substring(2, 4), 16) / 255;
            const g = parseInt(hex.substring(4, 6), 16) / 255;
            const b = parseInt(hex.substring(6, 8), 16) / 255;
            const start = document.positionAt(match.index);
            const end = document.positionAt(match.index + match[0].length);
            results.push(new vscode.ColorInformation(new vscode.Range(start, end), new vscode.Color(r, g, b, a)));
        }
        return results;
    },
    provideColorPresentations(color) {
        const text = `0x${toByte(color.alpha)}${toByte(color.red)}${toByte(color.green)}${toByte(color.blue)}`;
        return [new vscode.ColorPresentation(text)];
    }
};

function activate(context) {
    context.subscriptions.push(vscode.languages.registerColorProvider(['c', 'cpp'], provider));
}

function deactivate() {}

module.exports = { activate, deactivate };
