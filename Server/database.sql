-- Drop the database if it exists
DROP DATABASE IF EXISTS iotdb;

-- Create the database
CREATE DATABASE iotdb;
USE iotdb;

-- Create the Users table
CREATE TABLE `Users` (
    `userName` VARCHAR(255) NOT NULL,
    `passwordHash` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`userName`)
);
-- Create the Equipment table
CREATE TABLE `Equipment_Plug` (
    `equipmentID` VARCHAR(255) NOT NULL,
    `equipmentName` VARCHAR(255) NOT NULL,
    `numberSocket` INT NOT NULL,
    `position` VARCHAR(255) DEFAULT "BLANK",
    `manager` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (`equipmentID`),
    CONSTRAINT `fk_roomName_equipment` FOREIGN KEY (`manager`) REFERENCES `Users`(`userName`)
);
CREATE TABLE `Script` (
	`scriptName` VARCHAR(255) NOT NULL,
    `status` TINYINT DEFAULT 0 CHECK (status IN (0, 1)),
    PRIMARY KEY (`scriptName`)
);
CREATE TABLE `Socket` (
	`socketID` VARCHAR(255) NOT NULL,
    `socketName` VARCHAR(255) NOT NULL,
    `equipmentID` VARCHAR(255) NOT NULL,
    `status` TINYINT DEFAULT 0 CHECK (status IN (0, 1)),
    `scriptName` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (`socketID`),
    CONSTRAINT `fk_equipmentID` FOREIGN KEY (`equipmentID`) REFERENCES `Equipment_Plug`(`equipmentID`),
    CONSTRAINT `fk_scriptName` FOREIGN KEY (`scriptName`) REFERENCES `Script`(`scriptName`)
);
-- Create the DS_Temperature table
CREATE TABLE `DS_Temperature` (
    `sequenceID` VARCHAR(255) NOT NULL,
    `date` DATETIME NOT NULL,
    `dsID` VARCHAR(255) NOT NULL,
    `temperature` VARCHAR(255) NOT NULL,
    `equipmentID` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`sequenceID`),
    CONSTRAINT `fk_equipmentID_temperature` FOREIGN KEY (`equipmentID`) REFERENCES `Equipment_Plug`(`equipmentID`)
);

-- Create the ACS_Current table
CREATE TABLE `ACS_Current` (
    `seqID` VARCHAR(255) NOT NULL,
    `date` DATETIME NOT NULL,
    `acsID` VARCHAR(255) NOT NULL,
    `current` VARCHAR(255) NOT NULL,
    `equipmentID` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`seqID`),
    CONSTRAINT `fk_equipmentID_current` FOREIGN KEY (`equipmentID`) REFERENCES `Equipment_Plug`(`equipmentID`)
);

DELIMITER //
CREATE FUNCTION createUser(user_name VARCHAR(255), passwordHash VARCHAR(255))
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE user_exists INT;

    -- Check if the user already exists
    SELECT COUNT(*) INTO user_exists FROM Users WHERE userName = user_name;

    IF user_exists > 0 THEN
        RETURN 'User already exists';
    ELSE
        -- Insert the user if it doesn't exist
        INSERT INTO Users (userName, passwordHash) VALUES (user_name, passwordHash);
        RETURN CONCAT('User ', user_name, ' created');
    END IF;
END //

CREATE FUNCTION createEquipment(equipmentName VARCHAR(255), numSockets INT)
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE equip_id VARCHAR(36); -- Using VARCHAR to store UUID
    
    -- Generate UUID for equipmentID
    SET equip_id = UUID();

    -- Insert the equipment with the generated UUID
    INSERT INTO Equipment_Plug (equipmentID, equipmentName, numberSocket) 
    VALUES (equip_id, equipmentName, numSockets);

    RETURN CONCAT('Equipment created with ID: ', equip_id);
END //

CREATE FUNCTION changeManager(user_name VARCHAR(255), equip_id VARCHAR(255))
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE user_exists INT;
    DECLARE equip_exists INT;

    -- Check if the user exists in the Users table
    SELECT COUNT(*) INTO user_exists FROM Users WHERE userName = user_name;

    -- Check if the equipment exists
    SELECT COUNT(*) INTO equip_exists FROM Equipment_Plug WHERE equipmentID = equip_id;

    IF user_exists > 0 THEN
        IF equip_exists > 0 THEN
            -- Update the manager field in Equipment_Plug table for the specified equipmentID
            UPDATE Equipment_Plug SET manager = user_name WHERE equipmentID = equip_id;
            RETURN CONCAT('Manager updated to ', user_name);
        ELSE
            RETURN 'Equipment does not exist';
        END IF;
    ELSE
        RETURN 'User does not exist';
    END IF;
END //

CREATE FUNCTION updatePosition(new_position VARCHAR(255), equip_id VARCHAR(255))
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE equip_exists INT;

    -- Check if the equipment exists
    SELECT COUNT(*) INTO equip_exists FROM Equipment_Plug WHERE equipmentID = equip_id;

    IF equip_exists > 0 THEN
        -- Update the position field in Equipment_Plug table for the specified equipmentID
        UPDATE Equipment_Plug SET position = new_position WHERE equipmentID = equip_id;
        RETURN CONCAT('Position updated to ', new_position);
    ELSE
        RETURN 'Equipment does not exist';
    END IF;
END //

CREATE FUNCTION AddNewScript(script_name VARCHAR(255))
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE script_exists INT;

    -- Check if the script already exists
    SELECT COUNT(*) INTO script_exists FROM Script WHERE scriptName = script_name;

    IF script_exists > 0 THEN
        RETURN 'Script already exists';
    ELSE
        -- Insert the new script
        INSERT INTO Script (scriptName) VALUES (script_name);
        RETURN CONCAT('Script ', script_name, ' added successfully');
    END IF;
END //

CREATE FUNCTION AddSocketToScript(socket_id VARCHAR(255), scriptname VARCHAR(255))
RETURNS VARCHAR(255) DETERMINISTIC
BEGIN
    DECLARE socket_exists INT;
    DECLARE script_exists INT;

    -- Check if the socket exists
    SELECT COUNT(*) INTO socket_exists FROM Socket WHERE socketID = socket_id;

    -- Check if the script exists
    SELECT COUNT(*) INTO script_exists FROM Script WHERE scriptName = scriptname;

    IF socket_exists > 0 THEN
        IF script_exists > 0 OR scriptname IS NULL THEN
            -- Update the scriptName field in the Socket table for the specified socketID
            UPDATE Socket SET scriptName = scriptname WHERE socketID = socket_id;
            RETURN CONCAT('ScriptName updated to ', scriptname, ' for socketID: ', socket_id);
        ELSE
            RETURN 'Script does not exist';
        END IF;
    ELSE
        RETURN 'Socket does not exist';
    END IF;
END //


CREATE TRIGGER AutoCreateSocket AFTER INSERT ON Equipment_Plug
FOR EACH ROW
BEGIN
    DECLARE i INT DEFAULT 1;
    WHILE i <= NEW.numberSocket DO
        INSERT INTO Socket (socketID, socketName, equipmentID)
        VALUES (UUID(), CONCAT('Socket_', i), NEW.equipmentID);
        SET i = i + 1;
    END WHILE;
END //

CREATE TRIGGER UpdateSocketStatus AFTER UPDATE ON Script
FOR EACH ROW
BEGIN
    IF OLD.status != NEW.status THEN
        IF NEW.status = 1 THEN
            UPDATE Socket SET status = 1 WHERE scriptName = NEW.scriptName;
        ELSE
            UPDATE Socket SET status = 0 WHERE scriptName = NEW.scriptName;
        END IF;
    END IF;
END //


CREATE FUNCTION login(_username VARCHAR(255), _password VARCHAR(255)) RETURNS BOOLEAN
DETERMINISTIC
BEGIN
    -- DECLARE _hashedPassword VARCHAR(255);

    -- -- Assuming you have a function to hash passwords, replace 'HASH_FUNCTION' with the actual hashing function.
    -- SET _hashedPassword = HASH_FUNCTION(_password);

    -- Check if the provided username and hashed password match the records in the Users table.
    RETURN EXISTS (
        SELECT 1
        FROM Users
        WHERE userName = _username AND passwordHash = _password
    );
END //

DELIMITER ;
