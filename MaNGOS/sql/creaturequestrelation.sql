-- phpMyAdmin SQL Dump
-- version 2.6.2
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Sep 10, 2005 at 12:21 PM
-- Server version: 4.1.12
-- PHP Version: 5.0.4
-- 
-- Database: `mangos`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `creaturequestrelation`
-- 
-- Creation: Sep 10, 2005 at 12:02 PM
-- Last update: Sep 10, 2005 at 12:02 PM
-- 

DROP TABLE IF EXISTS `creaturequestrelation`;
CREATE TABLE IF NOT EXISTS `creaturequestrelation` (
  `Id` int(6) unsigned NOT NULL auto_increment,
  `questId` bigint(20) unsigned NOT NULL default '0',
  `creatureId` bigint(20) unsigned NOT NULL default '0',
  PRIMARY KEY  (`Id`)
) TYPE=MyISAM COMMENT='Maps which creatures have which quests; InnoDB free: 18432 k' AUTO_INCREMENT=1 ;
