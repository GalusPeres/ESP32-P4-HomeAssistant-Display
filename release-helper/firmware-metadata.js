function readNullTerminatedString(buffer, offset, maxLength) {
  const endLimit = Math.min(buffer.length, offset + maxLength);
  let end = offset;
  while (end < endLimit && buffer[end] !== 0) end += 1;
  return buffer.subarray(offset, end).toString('utf8');
}

function parseFirmwareMetadata(image, appOffset = 0) {
  const imageHeaderMagic = 0xe9;
  const appDescriptorMagic = 0xabcd5432;
  const deviceDescriptorMagic = 0x44565034;
  const siliconDescriptorMagic = 0x53525634;
  const deviceDescriptorOffset = appOffset + 24 + 8 + 256;
  const deviceDescriptorLength = 4 + 32 + 32 + 32;
  const siliconOffset = deviceDescriptorOffset + deviceDescriptorLength;
  const siliconLength = 4 + 2 + 2 + 16;

  if (image.length < deviceDescriptorOffset + deviceDescriptorLength) {
    throw new Error('Firmware image is too small for HomeTiles device metadata.');
  }
  if (image[appOffset] !== imageHeaderMagic) {
    throw new Error(`Firmware has no ESP image header at 0x${appOffset.toString(16)}.`);
  }
  if (image.readUInt32LE(appOffset + 24 + 8) !== appDescriptorMagic) {
    throw new Error('Firmware has no ESP application descriptor.');
  }
  if (image.readUInt32LE(deviceDescriptorOffset) !== deviceDescriptorMagic) {
    throw new Error('Firmware has no HomeTiles device descriptor.');
  }

  const metadata = {
    deviceKey: readNullTerminatedString(image, deviceDescriptorOffset + 4 + 32, 32),
    silicon: null,
  };
  if (
    image.length >= siliconOffset + siliconLength &&
    image.readUInt32LE(siliconOffset) === siliconDescriptorMagic
  ) {
    const silicon = {
      minimumRevision: image.readUInt16LE(siliconOffset + 4),
      maximumRevision: image.readUInt16LE(siliconOffset + 6),
      variant: readNullTerminatedString(image, siliconOffset + 8, 16),
    };
    if (silicon.minimumRevision > silicon.maximumRevision) {
      throw new Error(
        `Firmware has an invalid silicon revision range: ${silicon.minimumRevision}-${silicon.maximumRevision}`
      );
    }
    metadata.silicon = silicon;
  }
  return metadata;
}

function assertReleaseMetadata(metadata, profile) {
  if (metadata.deviceKey !== profile.metadataDeviceKey) {
    throw new Error('Firmware metadata device mismatch: expected ' + profile.metadataDeviceKey + ', got ' + metadata.deviceKey);
  }
  if (!metadata.silicon || metadata.silicon.variant !== profile.siliconVariant) {
    throw new Error('Firmware silicon variant mismatch: expected ' + profile.siliconVariant);
  }
  if (metadata.silicon.minimumRevision !== profile.minimumRevision || metadata.silicon.maximumRevision !== profile.maximumRevision) {
    throw new Error('Firmware silicon revision range mismatch: expected ' + profile.minimumRevision + '-' + profile.maximumRevision);
  }
}

module.exports = { parseFirmwareMetadata, assertReleaseMetadata };
